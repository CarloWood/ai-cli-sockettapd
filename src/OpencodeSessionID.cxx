#include "sys.h"
#include "OpencodeSessionID.h"
#include "utils/AIAlert.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <iterator>
#ifdef CWDEBUG
#include <chrono>
#include <iostream>
#include "utils/debug_ostream_operators.h"
#endif
#include "debug.h"

void OpencodeSessionID::assign_from_string(std::string_view const& sv)
{
  using namespace std::literals;

  if (!sv.starts_with("ses_"))
    THROW_ALERT("OpencodeSessionID must start with 'ses_'.");

  if (sv.length() != 30)
    THROW_ALERT("OpencodeSessionID has unexpected length [LEN].", AIArgs("[LEN]", sv.length()));

  std::string_view const timestamp_sv(sv.substr(4, 12));
  if (!std::ranges::all_of(timestamp_sv, [](char x) { return std::isxdigit(x) && !std::isupper(x); }))  // Only characters from 0123456789abcdef.
    THROW_ALERT("OpencodeSessionID has unexpected non-hex or uppercase characters");
  [[maybe_unused]] auto const result = std::from_chars(timestamp_sv.data(), timestamp_sv.data() + timestamp_sv.size(), timestamp_, 16);

  std::string_view const id_sv(sv.substr(16, 14));
  if (!std::ranges::all_of(id_sv, [](char x) { return std::isalnum(x); }))      // Only characters from 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.
    THROW_ALERT("OpencodeSessionID has unexpected non-base62 characters");
  std::memcpy(id_.data(), id_sv.begin(), id_.size());

  // In the end this conversion is only usable if it will reproduce the exact string upon converting it back to a string.
  // This is the canonical test.
  ASSERT(to_string() == sv);
}

std::string OpencodeSessionID::to_string() const
{
  return std::format("ses_{:012x}{}", timestamp_, std::string{id_.data(), id_.size()});
}

#ifdef CWDEBUG
void OpencodeSessionID::print_on(std::ostream& os) const
{
  using namespace std::chrono;
  auto tp = sys_time<milliseconds>{milliseconds{timestamp_}};
  os << '{' << std::format("{:%F %T} UTC", floor<seconds>(tp)) << ", " << std::string_view(id_.data(), id_.size()) << '}';
}
#endif
