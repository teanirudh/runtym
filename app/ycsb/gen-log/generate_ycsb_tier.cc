#include "uniform_random.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>

using UniformRand = foedus::assorted::UniformRandom;

static_assert(RD + WR <= 64);
static_assert(HT + WM + CD == RD + WR);

static constexpr uint64_t ROW_COUNT = 10'000'000;
static constexpr uint32_t HOT_KEYS = ROW_COUNT / 10000; // 0.01%
static constexpr uint32_t WARM_KEYS = 99 * ROW_COUNT / 10000; // 0.99%
static constexpr uint32_t COLD_KEYS = 99 * ROW_COUNT / 100; // 99%
static constexpr uint32_t TX_COUNT = 1'000'000;
static constexpr size_t PAD_SIZE = (64 - (((RD + WR) * 12 + 8) % 64)) % 64;

static_assert(HOT_KEYS + WARM_KEYS + COLD_KEYS == ROW_COUNT);

uint64_t COWN_PTRS[RD + WR] = {};
uint8_t PADDING[63] = {};

static uint32_t permute_key(uint32_t logical)
{
  return static_cast<uint32_t>((logical * 2654435761ull) % ROW_COUNT);
}

std::array<uint32_t, (RD + WR)> gen_keys(UniformRand* urand)
{
  std::array<uint32_t, (RD + WR)> logical;
  constexpr std::array<uint32_t, 3> tier_starts{
    0, HOT_KEYS, HOT_KEYS + WARM_KEYS};
  constexpr std::array<uint32_t, 3> tier_ends{
    HOT_KEYS - 1, HOT_KEYS + WARM_KEYS - 1, ROW_COUNT - 1};
  constexpr std::array<size_t, 3> tier_counts{HT, WM, CD};
  size_t index = 0;

  uint32_t hottest_key = ROW_COUNT, hottest_index = 0;
  for (size_t tier = 0; tier < tier_counts.size(); tier++)
  {
    for (size_t sampled = 0; sampled < tier_counts[tier]; sampled++)
    {
    again:
      logical[index] =
        urand->uniform_within(tier_starts[tier], tier_ends[tier]);
      if (logical[index] < hottest_key)
      {
        hottest_key = logical[index];
        hottest_index = index;
      }
      for (size_t previous = 0; previous < index; previous++)
        if (logical[index] == logical[previous])
          goto again;
      index++;
    }
  }

  std::swap(logical[0], logical[hottest_index]);

  std::array<uint32_t, (RD + WR)> keys;
  for (int i = 0; i < (RD + WR); i++)
    keys[i] = permute_key(logical[i]);

  return keys;
}

uint64_t gen_write_set()
{
  static std::mt19937 rng{SD};
  std::array<uint8_t, RD + WR> is_write{};
  std::fill(is_write.end() - WR, is_write.end(), 1);
  std::shuffle(is_write.begin(), is_write.end(), rng);

  uint64_t write_set = 0;
  for (size_t i = 0; i < is_write.size(); i++)
    write_set |= static_cast<uint64_t>(is_write[i]) << i;

  return write_set;
}

void gen_bin_txn(UniformRand* urand, std::ofstream* f)
{
  auto keys = gen_keys(urand);
  auto write_set = gen_write_set();

  for (const uint32_t& key : keys)
    f->write(reinterpret_cast<const char*>(&key), sizeof(uint32_t));

  f->write(reinterpret_cast<const char*>(&write_set), sizeof(uint64_t));
  f->write(reinterpret_cast<const char*>(COWN_PTRS), sizeof(COWN_PTRS));
  f->write(reinterpret_cast<const char*>(PADDING), PAD_SIZE);
}

int main()
{
  UniformRand urand(SD);

  char log[40];
  sprintf(log, "rd%02d_wr%02d_ht%02d_wm%02d_cd%02d.txt", RD, WR, HT, WM, CD);

  std::ofstream outlog(log, std::ios::binary);
  outlog.write(reinterpret_cast<const char*>(&TX_COUNT), sizeof(uint32_t));

  for (int i = 0; i < TX_COUNT; i++)
    gen_bin_txn(&urand, &outlog);

  outlog.close();
  return 0;
}
