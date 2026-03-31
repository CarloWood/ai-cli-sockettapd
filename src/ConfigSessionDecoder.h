#pragma once

#include "SessionID.h"
#include "evio/protocol/Decoder.h"
#include "evio/StreamBuf.h"
#include <string>

class STDecoder;

class ConfigSessionDecoder : public evio::protocol::Decoder
{
 private:
  SessionID session_id_;
  std::string agent_name_;
  STDecoder* return_decoder_ = nullptr;
  bool have_session_id_ = false;
  bool have_agent_name_ = false;

 public:
  void begin(STDecoder& return_decoder)
  {
    return_decoder_ = &return_decoder;
    have_session_id_ = false;
    have_agent_name_ = false;
  }

 protected:
  void decode(int& allow_deletion_count, evio::MsgBlock&& msg) override;
};
