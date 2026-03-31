#include "sys.h"
#include "STDecoder.h"
#include "ConfigSessionDecoder.h"
#include "Application.h"
#include "utils/AIAlert.h"
#include "debug.h"

namespace {

std::string xml_unescape(std::string_view input)
{
  std::string output;
  output.reserve(input.size());
  for (size_t i = 0; i < input.size();)
  {
    if (input[i] == '&')
    {
      if (input.compare(i, 5, "&amp;") == 0)
      {
        output += '&';
        i += 5;
        continue;
      }
      if (input.compare(i, 4, "&lt;") == 0)
      {
        output += '<';
        i += 4;
        continue;
      }
      if (input.compare(i, 4, "&gt;") == 0)
      {
        output += '>';
        i += 4;
        continue;
      }
      if (input.compare(i, 6, "&quot;") == 0)
      {
        output += '"';
        i += 6;
        continue;
      }
      if (input.compare(i, 6, "&apos;") == 0)
      {
        output += '\'';
        i += 6;
        continue;
      }
    }
    output += input[i++];
  }
  return output;
}

} // namespace

void ConfigSessionDecoder::decode(int& allow_deletion_count, evio::MsgBlock&& msg)
{
  DoutEntering(dc::notice, "ConfigSessionDecoder::decode({" << allow_deletion_count <<
      "}, \"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');

  constexpr std::string_view session_id_open = "<session-id>";
  constexpr std::string_view session_id_close = "</session-id>";
  constexpr std::string_view agent_open = "<agent>";
  constexpr std::string_view agent_close = "</agent>";

  std::string_view const line = msg.view();

  if (!have_session_id_)
  {
    auto const open_pos = line.find(session_id_open);
    if (open_pos != std::string_view::npos)
    {
      auto const value_pos = open_pos + session_id_open.size();
      auto const close_pos = line.find(session_id_close, value_pos);
      if (close_pos != std::string_view::npos && close_pos > value_pos)
      {
        std::string_view const session_id_sv = line.substr(value_pos, close_pos - value_pos);
        session_id_.assign_from_json_string(session_id_sv);
        have_session_id_ = true;
      }
      return;
    }
  }

  if (!have_agent_name_)
  {
    auto const open_pos = line.find(agent_open);
    if (open_pos != std::string_view::npos)
    {
      auto const value_pos = open_pos + agent_open.size();
      auto const close_pos = line.find(agent_close, value_pos);
      if (close_pos != std::string_view::npos && close_pos > value_pos)
      {
        std::string_view const agent_name_sv = line.substr(value_pos, close_pos - value_pos);
        agent_name_ = xml_unescape(agent_name_sv);
        have_agent_name_ = true;
      }
      return;
    }
  }

  if (line == "</config-session>\n")
  {
    if (!have_session_id_)
      THROW_LALERT("Received </config-session> without <session-id> block!");
    if (!have_agent_name_)
      THROW_LALERT("Received </config-session> without <agent> block!");

    // Call begin(return_decoder) before passing ConfigSessionDecoder the to switch_protocol_decoder.
    ASSERT(return_decoder_);
    switch_protocol_decoder(*return_decoder_);

    // Pass the decoded Thread ID back to STDecoder.
    return_decoder_->session_id_received(session_id_, agent_name_);
  }
}
