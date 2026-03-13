#pragma once

#include "ConfigSessionDecoder.h"
#include "evio/protocol/Decoder.h"

// Decoder for the accepted socket.
// ST = Socket Tap.
class STDecoder : public evio::protocol::Decoder
{
 public:
  STDecoder() = default;

  // Called by config_session_decoder_.
  void session_id_received(SessionID const& session_id);

 protected:
  // Call decode() with chunks ending on a newline (the default).
  void decode(int& allow_deletion_count, evio::MsgBlock&& msg) override;

 private:
  ConfigSessionDecoder config_session_decoder_;
};

