#include "sys.h"
#include "Sockettapd.h"
#include "utils/AIAlert.h"
#include "evio/Socket.h"
#include <cctype>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <sys/stat.h>   // utimensat
#include <fcntl.h>      // AT_FDCWD

Sockettapd::Sockettapd(int argc, char* argv[])
{
  Application::initialize(argc, argv);
}

Sockettapd::~Sockettapd()
{
#ifdef CWDEBUG
  // Make sure we don't write debug output to a closed file.
  if (!opt_foreground_ && !logfile_name_.empty())
  {
    Dout(dc::notice, "Turning all debug output off!");
    Debug(libcw_do.off());
  }
#endif
}

std::filesystem::path const& Sockettapd::workspace_root() const
{
  if (workspace_root_.empty())
  {
    char const* const workspace_root_env = ::getenv("WORKSPACE_ROOT");
    if (!workspace_root_env || !*workspace_root_env)
      THROW_ALERT("WORKSPACE_ROOT is not set and --workspace-root was not passed.");
    workspace_root_ = workspace_root_env;
  }
  return workspace_root_;
}

std::filesystem::path const& Sockettapd::planroot() const
{
  if (planroot_.empty())
  {
    char const* const planroot_env = ::getenv("PLANROOT");
    if (!planroot_env || !*planroot_env)
      THROW_ALERT("PLANROOT is not set and --planroot was not passed.");
    planroot_ = planroot_env;
  }
  return planroot_;
}

//virtual
void Sockettapd::command_line_parameters_parsed()
{
  DoutEntering(dc::notice, "Sockettapd::command_line_parameters_parsed()");

#ifdef CWDEBUG
  // Make logfile_name_ absolute.
  if (logfile_name_.is_relative())
    logfile_name_ = workspace_root() / logfile_name_;

  Dout(dc::notice, "logfile_name_ is now " << logfile_name_);
#endif

  // Switch to the background BEFORE creating any threads!
  if (!opt_foreground_)
    goto_background();
}

//virtual
bool Sockettapd::parse_command_line_parameter(std::string_view arg, int argc, char* argv[], int* index)
{
  DoutEntering(dc::notice, "Sockettapd::parse_command_line_parameter(\"" << arg << "\", " << argc << ", " << debug::print_argv(argv) << ", &" << index << ")");

  if (arg == "--one-shot")
  {
    opt_one_shot_ = true;
    return true;
  }

  if (arg == "--foreground")
  {
    opt_foreground_ = true;
    return true;
  }

  if (arg == "--socket")
  {
    ++*index;
    if (*index >= argc)
      THROW_ALERT("Missing argument for [ARG]", AIArgs("[ARG]", arg));
    socket_arg_ = argv[*index];
    if (socket_arg_.empty())
      THROW_ALERT("Empty value for [ARG].", AIArgs("[ARG]", arg));
    Dout(dc::notice, "socket_arg_ set to " << socket_arg_ << ".");
    return true;
  }

  if (arg == "--workspace-root" || arg == "--planroot" || arg == "--log")
  {
    ++*index;
    if (*index >= argc)
      THROW_ALERT("Missing argument for [ARG]", AIArgs("[ARG]", arg));
    if (arg == "--workspace-root")
    {
      workspace_root_ = argv[*index];
      Dout(dc::notice, "workspace_root_ set to " << workspace_root_ << ".");
    }
    else if (arg == "--planroot")
    {
      planroot_ = argv[*index];
      Dout(dc::notice, "planroot_ set to " << planroot_ << ".");
    }
#ifdef CWDEBUG
    else
    {
      logfile_name_ = argv[*index];
      Dout(dc::notice, "logfile_name_ set to " << logfile_name_ << ".");
    }
#endif
    return true;
  }

  return false;
}

//virtual
void Sockettapd::print_usage_extra(std::ostream& os) const
{
  os << "[--one-shot][--foreground][--workspace-root <dirname>][--planroot <dirname>][--log <logfile>][--socket <name>]";
}

//virtual
std::u8string Sockettapd::application_name() const
{
  return u8"sockettapd";
}

void Sockettapd::goto_background()
{
  DoutEntering(dc::notice, "Sockettapd::goto_background()");

  // Make sure we don't write to a closed ostream.
  Dout(dc::notice, "Turning all debug output off!");
  Debug(libcw_do.off());

  // Closes stdin/stdout/stderr and changes cwd to "/".
  // First argument must be 1 (do not change cwd) for `attach_gdb()` to work).
  if (::daemon(1, 0) == -1)
    THROW_ALERTE("daemon");

#ifdef CWDEBUG
  // We are now running in the background. Either keep debugging off or start writing to a log file.
  if (!logfile_name_.empty())
  {
    logfile_.open(logfile_name_);
    Debug(libcw_do.set_ostream(&logfile_, &logfile_mutex_));
    Debug(libcw_do.on());
    Dout(dc::notice, "Turned all debug output on again!");
  }
#endif
}

void touch_symlink(std::filesystem::path const& link_path)
{
  if (utimensat(AT_FDCWD, link_path.c_str(), nullptr, AT_SYMLINK_NOFOLLOW) == -1)
    THROW_ALERTE("utimensat failed for [PATH]", AIArgs("[PATH]", link_path.string()));
}

void Sockettapd::create_session_id_dir()
{
  // Create $WORKSPACE_ROOT/AAP/ThreadID/<session_id> and update $WORKSPACE_ROOT/AAP/{analyst,planner,coder}/id symlink if applicable.

  std::string const session_id_str = session_id_.value().to_string();
  std::filesystem::path const thread_dir = planroot() / "ThreadID" / session_id_str;

  std::error_code ec;
  if (!std::filesystem::exists(thread_dir, ec) || ec)
  {
    if (!ec)
      std::filesystem::create_directories(thread_dir, ec);
    if (ec)
      THROW_ALERT("Failed to create directory [DIR]: [ERROR].", AIArgs("[DIR]", thread_dir.string())("[ERROR]", ec.message()));
  }

  if (agent_name_ == "analyst" || agent_name_ == "planner" || agent_name_ == "coder")
  {
    // Record the current (primary) agent for this thread directory.
    {
      std::filesystem::path const mode_path = thread_dir / "mode";
      std::ofstream out(mode_path, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!out)
        THROW_ALERT("Failed to open [PATH] for writing.", AIArgs("[PATH]", mode_path.string()));
      out << agent_name_ << "\n";
      if (!out)
        THROW_ALERT("Failed to write [PATH].", AIArgs("[PATH]", mode_path.string()));
    }

    std::filesystem::path const link_path = planroot() / agent_name_ / "id";
    std::filesystem::path const relative_target = std::filesystem::path("..") / "ThreadID" / session_id_str;
    std::filesystem::create_directories(link_path.parent_path(), ec);
    if (ec)
      THROW_ALERT("Failed to create directory [DIR]: [ERROR].", AIArgs("[DIR]", link_path.parent_path().string())("[ERROR]", ec.message()));
    if (std::filesystem::exists(link_path, ec))
    {
      if (ec)
        THROW_ALERT("Failed to check existence of [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));

      if (!std::filesystem::is_symlink(link_path, ec))
      {
        if (ec)
          THROW_ALERT("Failed to stat [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));
        THROW_ALERT("[PATH] exists but is not a symlink.", AIArgs("[PATH]", link_path.string()));
      }

      std::filesystem::path const target = std::filesystem::read_symlink(link_path, ec);
      if (ec)
        THROW_ALERT("Failed to read symlink [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));

      std::filesystem::path const target_abs = std::filesystem::weakly_canonical(link_path.parent_path() / target, ec);
      if (ec)
        THROW_ALERT("Failed to canonicalize symlink target [TARGET]: [ERROR].", AIArgs("[TARGET]", target.string())("[ERROR]", ec.message()));
      std::filesystem::path const thread_dir_abs = std::filesystem::weakly_canonical(thread_dir, ec);
      if (ec)
        THROW_ALERT("Failed to canonicalize directory [DIR]: [ERROR].", AIArgs("[DIR]", thread_dir.string())("[ERROR]", ec.message()));

      if (target_abs != thread_dir_abs)
      {
        std::filesystem::remove(link_path, ec);
        if (ec)
          THROW_ALERT("Failed to remove symlink [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));
        std::filesystem::create_directory_symlink(relative_target, link_path, ec);
        if (ec)
          THROW_ALERT("Failed to create symlink [PATH] -> [TARGET]: [ERROR].",
              AIArgs("[PATH]", link_path.string())("[TARGET]", relative_target.string())("[ERROR]", ec.message()));
      }
      else
        touch_symlink(link_path);
    }
    else
    {
      if (ec)
        THROW_ALERT("Failed to check existence of [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));
      std::filesystem::create_directory_symlink(relative_target, link_path, ec);
      if (ec)
        THROW_ALERT("Failed to create symlink [PATH] -> [TARGET]: [ERROR].",
            AIArgs("[PATH]", link_path.string())("[TARGET]", relative_target.string())("[ERROR]", ec.message()));
    }
  }
}

void Sockettapd::received_session_id(SessionID const& session_id, std::string const& agent_name, evio::Socket& client)
{
  DoutEntering(dc::notice, "Sockettapd::received_session_id(" << session_id << ", \"" << agent_name << "\", " << &client << ")");

  boost::intrusive_ptr<evio::Socket> new_client(&client);

  session_id_ = session_id;
  agent_name_ = agent_name;
  create_session_id_dir();

  // Same thread id: keep the newest connection and drop any older still-connected client.
  if (client_ && client_.get() != new_client.get())
    client_->close();
  client_ = std::move(new_client);
}
