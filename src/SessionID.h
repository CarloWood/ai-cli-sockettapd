#pragma once

#include "UUID.h"
#include "OpencodeSessionID.h"
#include <boost/uuid/uuid_io.hpp>

class SessionID
{
 private:
  UUID uuid_;

 public:
  void assign_from_json_string(std::string_view const& session_id_data)
  {
    uuid_.assign_from_json_string(session_id_data);
  }

  std::string to_string() const
  {
    return boost::uuids::to_string(uuid_);
  }

  friend bool operator!=(SessionID const& lhs, SessionID const& rhs)
  {
    return lhs.uuid_ != rhs.uuid_;
  }

#ifdef CWDEBUG
  void print_on(std::ostream& os) const
  {
    uuid_.print_on(os);
  }
#endif
};
