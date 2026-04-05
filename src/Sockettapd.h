#pragma once

#include "Application.h"
#include "SessionID.h"
#include <boost/intrusive_ptr.hpp>
#include <filesystem>
#include <optional>
#include <string>
#ifdef CWDEBUG
#include <mutex>
#endif

namespace evio {
class Socket;
} // namespace evio

// Sockettapd
//
// Daemon application entrypoint.
class Sockettapd final : public Application
{
 private:
  bool opt_foreground_{false};                          // Set if --foreground.
  bool opt_one_shot_{false};                            // Set if --one-shot.
  mutable std::filesystem::path workspace_root_;        // Argument passed to --workspace-root <dir>, or read from $WORKSPACE_ROOT.
  mutable std::filesystem::path planroot_;              // Argument passed to --planroot <dir>, or read from $PLANROOT.
  std::string socket_arg_{ "shell_exec" };              // Argument passed to --socket <arg>.
  std::optional<SessionID> session_id_;                 // Set by received_session_id().
  std::string agent_name_;                              // Set by received_session_id().
  boost::intrusive_ptr<evio::Socket> client_;           // Current client for session_id_ (if any).
#ifdef CWDEBUG
  std::filesystem::path logfile_name_;                  // Argument passed to --log <file>.
  std::mutex logfile_mutex_;                            // Used to protect the logfile.
  std::ofstream logfile_;                               // Log file, opened if --log is given and the daemon runs in the background.
#endif

 private:
  void create_session_id_dir();

 public:
  // Construct and initialize base application state.
  Sockettapd(int argc, char* argv[]);

  // Destroy sockettapd object.
  ~Sockettapd();

  // Run as daemon.
  void goto_background();

  // Called when a thread ID was received through the <config-session>...</config-session> message.
  void received_session_id(SessionID const& session_id, std::string const& agent_name, evio::Socket& client);

  // Get application instance.
  static Sockettapd& instance() { return static_cast<Sockettapd&>(Application::instance()); }

  // Option accessors.
  bool one_shot() const { return opt_one_shot_; }
  bool foreground() const { return opt_foreground_; }
  std::string socket_name() const { return socket_arg_ + ".sock"; }
  std::filesystem::path const& workspace_root() const;
  std::filesystem::path const& planroot() const;

 protected:
  // Parse remountd-specific command line parameters.
  bool parse_command_line_parameter(std::string_view arg, int argc, char* argv[], int* index) override;

  // Called after all command line parameters were parsed.
  void command_line_parameters_parsed() override;

  // Print sockettapd-specific usage suffix.
  void print_usage_extra(std::ostream& os) const override;

  // Return the application display name.
  std::u8string application_name() const override;
};
