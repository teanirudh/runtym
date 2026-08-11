#pragma once

#include <stdint.h>

static constexpr uint32_t ROWS_PER_TX = RD + WR;
static constexpr uint32_t ROW_SIZE = 900;
static constexpr uint32_t WRITE_SIZE = 100;
static const uint64_t DB_SIZE = 10'000'000;
static constexpr uint32_t COWN_SLOT = 1024;
// const uint64_t PENDING_THRESHOLD = 100'000;
// const uint64_t SPAWN_THRESHOLD = 100'000;
