#include "pipeline.hpp"
#include "txcounter.hpp"
#include "ycsb/constants.hpp"
#include "ycsb/db.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>
#include <utility>

static std::atomic<uint64_t> exec_time{0};
static std::atomic<uint64_t> exec_count{0};

#ifdef LOG_LATENCY
#  define M_LOG_LATENCY() \
    { \
      TxCounter::instance().log_latency(init_time); \
      TxCounter::instance().incr(); \
    }
#else
#  define M_LOG_LATENCY() \
    { \
      TxCounter::instance().incr(); \
    }
#endif

struct YCSBRow
{
  char payload[ROW_SIZE];
};

static constexpr size_t TXN_RAW = ROWS_PER_TX * 12u + sizeof(uint64_t);
static constexpr size_t PAD_SIZE = (64 - (TXN_RAW % 64)) % 64;
static constexpr size_t TXN_BYTES = TXN_RAW + PAD_SIZE;

struct __attribute__((packed)) YCSBTransactionMarshalled
{
  uint32_t indices[ROWS_PER_TX];
  uint64_t write_set;
  uint64_t cown_ptrs[ROWS_PER_TX];
};

static_assert(sizeof(YCSBTransactionMarshalled) == TXN_RAW);
static_assert(RD + WR <= 64);

static inline void simulate_work_us(long us)
{
  long next_ts = time_ns() + us * 1000;
  while (time_ns() < next_ts)
    _mm_pause();
}

struct YCSBTransaction
{
public:
  static Index<YCSBRow>* index;

  static int prepare_cowns(char* input)
  {
    auto txm = reinterpret_cast<YCSBTransactionMarshalled*>(input);

    for (uint32_t i = 0; i < ROWS_PER_TX; i++)
    {
      auto&& cown = index->get_row(txm->indices[i]);
      txm->cown_ptrs[i] = cown.get_base_addr();
    }

    return static_cast<int>(TXN_BYTES);
  }

  static int prefetch_cowns(const char* input)
  {
    auto txm = reinterpret_cast<const YCSBTransactionMarshalled*>(input);

    for (uint32_t i = 0; i < ROWS_PER_TX; i++)
      __builtin_prefetch(
        reinterpret_cast<const void*>(txm->cown_ptrs[i]), 1, 3);

    return static_cast<int>(TXN_BYTES);
  }

  template<size_t... R, size_t... W>
  static void spawn(
    std::array<cown_ptr<YCSBRow>, ROWS_PER_TX>& rows,
#ifdef RPC_LATENCY
    ts_type init_time,
#endif
    std::index_sequence<R...>,
    std::index_sequence<W...>)
  {
#ifdef RPC_LATENCY
    when(read(rows[R])..., rows[RD + W]...) << [init_time](auto&&... acq)
#else
    when(read(rows[R])..., rows[RD + W]...) << [](auto&&... acq)
#endif
    {
      uint8_t sum = 0;
      auto op = [&](auto& a) {
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(*a)>>)
        {
          for (int j = 0; j < ROW_SIZE; j++)
            sum += a->payload[j];
        }
        else
        {
          memset(a->payload, sum, WRITE_SIZE);
        }
      };
      auto ts = time_ns();
      (op(acq), ...);
      auto te = time_ns();
      exec_time.fetch_add(te - ts, std::memory_order_relaxed);
      exec_count.fetch_add(1, std::memory_order_relaxed);
      M_LOG_LATENCY();
    };
  }

#ifdef RPC_LATENCY
  static int parse_and_process(const char* input, ts_type init_time)
#else
  static int parse_and_process(const char* input)
#endif
  {
    const auto* txm = reinterpret_cast<const YCSBTransactionMarshalled*>(input);

    std::array<cown_ptr<YCSBRow>, ROWS_PER_TX> rows{};
    for (uint32_t i = 0; i < ROWS_PER_TX; i++)
    {
#if defined(INDEXER) || defined(TEST_TWO)
      rows[i] = get_cown_ptr_from_addr<YCSBRow>(
        reinterpret_cast<void*>(txm->cown_ptrs[i]));
#else
      rows[i] = *index->get_row_addr(txm->indices[i]);
#endif
    }

    spawn(
      rows,
#ifdef RPC_LATENCY
      init_time,
#endif
      std::make_index_sequence<RD>{},
      std::make_index_sequence<WR>{});

    return static_cast<int>(TXN_BYTES);
  }

  YCSBTransaction(const YCSBTransaction&) = delete;
  YCSBTransaction& operator=(const YCSBTransaction&) = delete;
};

Index<YCSBRow>* YCSBTransaction::index;

int main(int argc, char** argv)
{
  if (argc != 6 || strcmp(argv[1], "-n") != 0)
  {
    fprintf(
      stderr,
      "Usage: ./program -n core_cnt"
      " <dispatcher_input_file> -i <inter_arrival>\n");
    return -1;
  }

  uint8_t core_cnt = atoi(argv[2]);
  uint8_t max_core = std::thread::hardware_concurrency();
  assert(1 < core_cnt && core_cnt <= max_core);

  YCSBTransaction::index = new Index<YCSBRow>;
  uint64_t cown_prev_addr = 0;
  uint8_t* cown_arr_addr =
    static_cast<uint8_t*>(aligned_alloc_hpage(COWN_SLOT * DB_SIZE));

  for (uint64_t i = 0; i < DB_SIZE; i++)
  {
    cown_ptr<YCSBRow> cown_r = make_cown_custom<YCSBRow>(
      reinterpret_cast<void*>(cown_arr_addr + (uint64_t)COWN_SLOT * i));

    if (i > 0)
      assert((cown_r.get_base_addr() - cown_prev_addr) == COWN_SLOT);
    cown_prev_addr = cown_r.get_base_addr();

    YCSBTransaction::index->insert_row(cown_r);
  }

  build_pipelines<YCSBTransaction>(core_cnt - 1, argv[3], argv[5]);

  const uint64_t n = exec_count.load(std::memory_order_relaxed);
  const uint64_t t = exec_time.load(std::memory_order_relaxed);
  if (n > 0)
  {
    auto mean_exec = static_cast<double>(t) / static_cast<double>(n) / 1000.0;
    printf("mean_txn_exec: %.3f us\n", mean_exec);
  }
}
