#include "sys.h"
#include "Sockettapd.h"
#include "STDecoder.h"
#include "debug.h"
#include "evio/Socket.h"

void STDecoder::decode(int& allow_deletion_count, evio::MsgBlock&& msg)
{
  // Just print what was received.
  DoutEntering(dc::notice, "STDecoder::decode({" << allow_deletion_count <<
      "}, \"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');

  std::string_view const line = msg.view();
  if (line.starts_with("<config-session>"))
  {
    config_session_decoder_.begin(*this);
    switch_protocol_decoder(config_session_decoder_);
  }
}

void STDecoder::session_id_received(SessionID const& session_id, std::string const& agent_name)
{
  DoutEntering(dc::notice, "STDecoder::session_id_received(" << session_id << ", \"" << agent_name << "\") [" << this << ']');

  evio::Socket* const client = static_cast<evio::Socket*>(m_input_device);
  Sockettapd::instance().received_session_id(session_id, agent_name, *client);
}
