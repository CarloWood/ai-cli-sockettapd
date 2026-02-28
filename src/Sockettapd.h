#pragma once

#include "Application.h"
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
  bool opt_foreground_{false};                  // Set if --foreground.
  bool opt_one_shot_{false};                    // Set if --one-shot.
  std::filesystem::path project_dir_;           // Argument passed to --projectdir <dir>.
  std::string socket_arg_{ "shell_exec" };      // Argument passed to --socket <arg>.
  std::optional<UUID> thread_id_;               // Set once by received_thread_id().
  boost::intrusive_ptr<evio::Socket> client_;   // Current client for thread_id_ (if any).
#ifdef CWDEBUG
  std::filesystem::path logfile_name_;          // Argument passed to --log <file>.
  std::mutex logfile_mutex_;                    // Used to protect the logfile.
  std::ofstream logfile_;                       // Log file, opened if --log is given and the daemon runs in the background.
#endif

 public:
  // Construct and initialize base application state.
  Sockettapd(int argc, char* argv[]);

  // Destroy sockettapd object.
  ~Sockettapd();

  // Run as daemon.
  void goto_background();

  // Called when a thread ID was received through the <config-session>...</config-session> message.
  void received_thread_id(UUID const& thread_id, evio::Socket& client);

  // Get application instance.
  static Sockettapd& instance() { return static_cast<Sockettapd&>(Application::instance()); }

  // Option accessors.
  bool one_shot() const { return opt_one_shot_; }
  bool foreground() const { return opt_foreground_; }
  std::string socket_name() const { return socket_arg_ + ".sock"; }

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
