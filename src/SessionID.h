#pragma once

#include "utils/has_print_on.h"
#include "UUID.h"
#include "OpencodeSessionID.h"
#include <boost/uuid/uuid_io.hpp>
#include <iosfwd>
#include <type_traits>
#include <variant>

class SessionID
{
 private:
  std::variant<UUID, OpencodeSessionID> session_id_;

 public:
  void assign_from_string(std::string_view const& sv)
  {
    if (sv.starts_with("ses_"))
      session_id_.emplace<OpencodeSessionID>().assign_from_string(sv);
    else
      session_id_.emplace<UUID>().assign_from_string(sv);
  }

  void assign_from_json_string(std::string_view const& session_id_data)
  {
    if (session_id_data.starts_with("ses_"))
      session_id_.emplace<OpencodeSessionID>().assign_from_json_string(session_id_data);
    else
      session_id_.emplace<UUID>().assign_from_json_string(session_id_data);
  }

  std::string to_string() const
  {
    return std::visit(
        [](auto const& session_id) -> std::string
        {
          using SessionIDType = std::decay_t<decltype(session_id)>;
          if constexpr (std::is_same_v<SessionIDType, UUID>)
            return boost::uuids::to_string(session_id);
          else
            return session_id.to_string();
        }, session_id_);
  }

  friend bool operator!=(SessionID const& lhs, SessionID const& rhs)
  {
    return lhs.session_id_ != rhs.session_id_;
  }

#ifdef CWDEBUG
  void print_on(std::ostream& os) const
  {
    std::visit([&os](auto const& session_id) { session_id.print_on(os); }, session_id_);
  }
#endif
};
