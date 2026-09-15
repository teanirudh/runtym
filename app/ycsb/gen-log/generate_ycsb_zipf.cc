#include "zipfian_random.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>

using Rand = foedus::assorted::ZipfianRandom;

static_assert(RD + WR <= 64);
static_assert(HK <= RD + WR);
static_assert(ZT < 100);

static constexpr uint64_t ROW_COUNT = 10'000'000;
static constexpr uint32_t TX_COUNT = 1'000'000;
static constexpr int HOT_KEY_MSB_BITS = 6;
static constexpr size_t PAD_SIZE = (64 - (((RD + WR) * 12 + 8) % 64)) % 64;

uint64_t COWN_PTRS[RD + WR] = {};
uint8_t PADDING[63] = {};

std::array<uint32_t, (RD + WR)> gen_keys(Rand* r)
{
  int nr_lsb = 63 - __builtin_clzll(ROW_COUNT) - HOT_KEY_MSB_BITS;
  size_t mask = 0;
  if (nr_lsb > 0)
    mask = (1 << nr_lsb) - 1;

  std::array<uint32_t, (RD + WR)> keys;
  for (int i = 0; i < (RD + WR); i++)
  {
  again:
    keys[i] = r->next() % ROW_COUNT;

    if (i < HK)
      keys[i] &= ~mask;
    else if ((keys[i] & mask) == 0)
      goto again;

    for (int j = 0; j < i; j++)
      if (keys[i] == keys[j])
        goto again;
  }

  return keys;
}

uint64_t gen_write_set()
{
  std::mt19937 g(std::random_device{}());
  std::array<uint8_t, RD + WR> is_write{};
  std::fill(is_write.end() - WR, is_write.end(), 1);
  std::shuffle(is_write.begin(), is_write.end(), g);

  uint64_t write_set = 0;
  for (uint8_t bit : is_write)
    write_set = (write_set << 1) | bit;

  return write_set;
}

// indices (4N) + write_set (8) + cown_ptrs (8N) + padding
void gen_bin_txn(Rand* rand, std::ofstream* f)
{
  auto keys = gen_keys(rand);
  auto write_set = gen_write_set();

  for (const uint32_t& key : keys)
    f->write(reinterpret_cast<const char*>(&key), sizeof(uint32_t));

  f->write(reinterpret_cast<const char*>(&write_set), sizeof(uint64_t));

  f->write(reinterpret_cast<const char*>(COWN_PTRS), sizeof(COWN_PTRS));

  f->write(reinterpret_cast<const char*>(PADDING), PAD_SIZE);
}

int main(int argc, char** argv)
{
  Rand rand;
  double zipf_theta = (double)ZT / 100;
  rand.init(ROW_COUNT, zipf_theta, 1234);

  char rd_wr[10], zt_hk[10];
  snprintf(rd_wr, sizeof(rd_wr), "rd%02d_wr%02d", RD, WR);
  snprintf(zt_hk, sizeof(zt_hk), "zt%02d_hk%02d", ZT, HK);

  char log_name[30];
  snprintf(log_name, sizeof(log_name), "%s_%s.txt", rd_wr, zt_hk);

  std::ofstream outLog(log_name, std::ios::binary);
  outLog.write(reinterpret_cast<const char*>(&TX_COUNT), sizeof(uint32_t));

  for (int i = 0; i < TX_COUNT; i++)
    gen_bin_txn(&rand, &outLog);

  outLog.close();
  return 0;
}
