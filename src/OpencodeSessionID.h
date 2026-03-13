#pragma once

#include "utils/has_print_on.h"
#include <string_view>
#include <cstdint>
#include <array>
#ifdef CWDEBUG
#include <iosfwd>
#endif

// This class defines a print_on method.
using utils::has_print_on::operator<<;

class OpencodeSessionID
{
 private:
  uint64_t timestamp_;          // 48-bits timestamp.
  std::array<char, 14> id_;     // base-62 random ID.

 public:
  // Default constructor creates an uninitialized OpencodeSessionID object!
  OpencodeSessionID() { }
  // Generate from string view.
  OpencodeSessionID(std::string_view const& sv)
  {
    assign_from_string(sv);
  }

  void assign_from_string(std::string_view const& sv);

  void assign_from_json_string(std::string_view const& session_id_data)
  {
    // OpencodeSessionID does not need xml unescaping, since it does not contain any of '"<>&.
    assign_from_string(session_id_data);
  }

  std::string to_string() const;

#ifdef CWDEBUG
  void print_on(std::ostream& os) const;
#endif
};
