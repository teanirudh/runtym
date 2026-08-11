#include "zipfian_random.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

using Rand = foedus::assorted::ZipfianRandom;

static_assert(RD + WR <= 64);
static_assert(HR <= RD && HW <= WR);

static constexpr uint64_t ROW_COUNT = 10'000'000;
static constexpr uint32_t TX_COUNT = 1'000'000;
static constexpr int HOT_KEY_MSB_BITS = 6;
static constexpr size_t PAD_SIZE = (64 - (((RD + WR) * 12 + 8) % 64)) % 64;
static constexpr uint64_t WRITE_SET =
  WR == 0 ? 0 : ((UINT64_MAX >> (64 - WR)) << RD);

uint64_t COWN_PTRS[RD + WR] = {};
uint8_t PADDING[63] = {};

// slots [0, RD) → reads (HR hot keys)
// slots [RD, RD+WR) → writes (HW hot keys)
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

    const bool is_read = i < RD;
    const bool is_hot = is_read ? (i < HR) : ((i - RD) < HW);

    if (is_hot)
      keys[i] &= ~mask;
    else if ((keys[i] & mask) == 0)
      goto again;

    for (int j = 0; j < i; j++)
      if (keys[i] == keys[j])
        goto again;
  }

  return keys;
}

// indices (4N) + write_set (8) + cown_ptrs (8N) + padding
void gen_bin_txn(Rand* rand, std::ofstream* f)
{
  auto keys = gen_keys(rand);
  for (const uint32_t& key : keys)
    f->write(reinterpret_cast<const char*>(&key), sizeof(uint32_t));

  f->write(reinterpret_cast<const char*>(&WRITE_SET), sizeof(uint64_t));

  f->write(reinterpret_cast<const char*>(COWN_PTRS), sizeof(COWN_PTRS));

  f->write(reinterpret_cast<const char*>(PADDING), PAD_SIZE);
}

int main(int argc, char** argv)
{
  Rand rand;
  rand.init(ROW_COUNT, 0, 1238);

  char log_name[50];
  snprintf(log_name, sizeof(log_name), "rd%02d_wr%02d.txt", RD, WR);

  std::ofstream outLog(log_name, std::ios::binary);
  outLog.write(reinterpret_cast<const char*>(&TX_COUNT), sizeof(uint32_t));

  for (int i = 0; i < TX_COUNT; i++)
    gen_bin_txn(&rand, &outLog);

  outLog.close();
  return 0;
}
