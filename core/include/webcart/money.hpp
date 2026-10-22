#pragma once

#include <cstdint>
#include <string>

namespace webcart {

// All amounts are integer euro cents. Floating point never touches money.
using Cents = std::int64_t;

// value * num / den rounded half away from zero.
Cents mul_div_round(Cents value, std::int64_t num, std::int64_t den);

// 1234 -> "12.34", -5 -> "-0.05"
std::string format_cents(Cents value);

}  // namespace webcart
