#pragma once

#include <cstdint>

static constexpr uint32_t kIA32_EFER = 0xC000'0080;
static constexpr uint32_t kIA32_STAR = 0xC000'0081;
static constexpr uint32_t kIA32_LSTAR = 0xC000'0082;
static constexpr uint32_t kIA32_FMASK = 0xC000'0084;