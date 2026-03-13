#pragma once

#include "SessionID.h"
#include "evio/protocol/Decoder.h"
#include "evio/StreamBuf.h"

class STDecoder;

class ConfigSessionDecoder : public evio::protocol::Decoder
{
 private:
  SessionID session_id_;
  STDecoder* return_decoder_ = nullptr;
  bool have_session_id_ = false;

 public:
  void begin(STDecoder& return_decoder)
  {
    return_decoder_ = &return_decoder;
    have_session_id_ = false;
  }

 protected:
  void decode(int& allow_deletion_count, evio::MsgBlock&& msg) override;
};
