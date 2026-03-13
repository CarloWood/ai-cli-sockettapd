#include "sys.h"
#include "Sockettapd.h"
#include "utils/AIAlert.h"
#include "evio/Socket.h"
#include <cctype>
#include <fstream>
#include <iterator>
#include <unistd.h>

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

std::filesystem::path const& Sockettapd::projectdir() const
{
  if (projectdir_.empty())
  {
    char const* const projectdir_env = ::getenv("PROJECTDIR");
    if (!projectdir_env || !*projectdir_env)
      THROW_ALERT("PROJECTDIR is not set and --projectdir was not passed.");
    projectdir_ = projectdir_env;
  }
  return projectdir_;
}

//virtual
void Sockettapd::command_line_parameters_parsed()
{
  DoutEntering(dc::notice, "Sockettapd::command_line_parameters_parsed()");

#ifdef CWDEBUG
  // Make logfile_name_ absolute.
  if (logfile_name_.is_relative())
    logfile_name_ = projectdir() / logfile_name_;

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

  if (arg == "--projectdir" || arg == "--log")
  {
    ++*index;
    if (*index >= argc)
      THROW_ALERT("Missing argument for [ARG]", AIArgs("[ARG]", arg));
    if (arg == "--projectdir")
    {
      projectdir_ = argv[*index];
      Dout(dc::notice, "projectdir_ set to " << projectdir_ << ".");
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
  os << "[--one-shot][--foreground][--projectdir <dirname>][--log <logfile>][--socket <name>]";
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

void Sockettapd::create_session_id_dir(evio::Socket& client)
{
  // Create $PROJECTDIR/AAP/ThreadID/<session_id> and update $PROJECTDIR/AAP/{planner,coder}/id symlink if applicable.

  std::string const session_id_str = session_id_.value().to_string();
  std::filesystem::path const thread_dir = projectdir() / "AAP" / "ThreadID" / session_id_str;
  {
    std::error_code ec;
    std::filesystem::create_directories(thread_dir, ec);
    if (ec)
      THROW_ALERT("Failed to create directory [DIR]: [ERROR].", AIArgs("[DIR]", thread_dir.string())("[ERROR]", ec.message()));
  }

  // Record (or verify) the mode of this thread directory.
  // A thread can only be owned by one daemon mode (socket_arg_).
  {
    std::filesystem::path const mode_path = thread_dir / "mode";
    std::error_code ec;
    if (std::filesystem::exists(mode_path, ec))
    {
      if (ec)
        THROW_ALERT("Failed to check existence of [PATH]: [ERROR].", AIArgs("[PATH]", mode_path.string())("[ERROR]", ec.message()));

      if (!std::filesystem::is_regular_file(mode_path, ec))
      {
        if (ec)
          THROW_ALERT("Failed to stat [PATH]: [ERROR].", AIArgs("[PATH]", mode_path.string())("[ERROR]", ec.message()));
        THROW_ALERT("[PATH] exists but is not a regular file.", AIArgs("[PATH]", mode_path.string()));
      }

      std::ifstream in(mode_path, std::ios::in | std::ios::binary);
      if (!in)
        THROW_ALERT("Failed to open [PATH] for reading.", AIArgs("[PATH]", mode_path.string()));

      std::string mode_value((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      if (!in.good() && !in.eof())
        THROW_ALERT("Failed to read [PATH].", AIArgs("[PATH]", mode_path.string()));

      auto trim = [](std::string& s) {
        auto const is_space = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && is_space(static_cast<unsigned char>(s.back())))
          s.pop_back();
        size_t start = 0;
        while (start < s.size() && is_space(static_cast<unsigned char>(s[start])))
          ++start;
        if (start > 0)
          s.erase(0, start);
      };
      trim(mode_value);

      if (mode_value != socket_arg_)
      {
        client.close();
        THROW_ALERT("ThreadID directory [DIR] is already in mode [MODE] (expected [EXPECTED]).",
            AIArgs("[DIR]", thread_dir.string())("[MODE]", mode_value)("[EXPECTED]", socket_arg_));
      }
    }
    else
    {
      if (ec)
        THROW_ALERT("Failed to check existence of [PATH]: [ERROR].", AIArgs("[PATH]", mode_path.string())("[ERROR]", ec.message()));

      std::ofstream out(mode_path, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!out)
        THROW_ALERT("Failed to open [PATH] for writing.", AIArgs("[PATH]", mode_path.string()));
      out << socket_arg_ << "\n";
      if (!out)
        THROW_ALERT("Failed to write [PATH].", AIArgs("[PATH]", mode_path.string()));
    }
  }

  if (socket_arg_ == "planner" || socket_arg_ == "coder")
  {
    std::filesystem::path const link_path = projectdir() / "AAP" / socket_arg_ / "id";
    std::error_code ec;
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
        std::filesystem::create_directory_symlink(thread_dir, link_path, ec);
        if (ec)
          THROW_ALERT("Failed to create symlink [PATH] -> [TARGET]: [ERROR].",
              AIArgs("[PATH]", link_path.string())("[TARGET]", thread_dir.string())("[ERROR]", ec.message()));
      }
    }
    else
    {
      if (ec)
        THROW_ALERT("Failed to check existence of [PATH]: [ERROR].", AIArgs("[PATH]", link_path.string())("[ERROR]", ec.message()));
      std::filesystem::create_directory_symlink(thread_dir, link_path, ec);
      if (ec)
        THROW_ALERT("Failed to create symlink [PATH] -> [TARGET]: [ERROR].",
            AIArgs("[PATH]", link_path.string())("[TARGET]", thread_dir.string())("[ERROR]", ec.message()));
    }
  }
}

void Sockettapd::received_session_id(SessionID const& session_id, evio::Socket& client)
{
  DoutEntering(dc::notice, "Sockettapd::received_session_id(" << session_id << ", " << &client << ")");

  boost::intrusive_ptr<evio::Socket> new_client(&client);

  if (!session_id_)
  {
    session_id_ = session_id;
    create_session_id_dir(client);
    client_ = std::move(new_client);
    return;
  }

  if (*session_id_ != session_id)
  {
    // This client is not compatible with the thread that owns this daemon.
    client.close();
    THROW_ALERT("Received a different Thread ID ([NEW]) than expected ([OLD]).",
        AIArgs("[NEW]", session_id.to_string())("[OLD]", session_id_->to_string()));
  }

  // Same thread id: keep the newest connection and drop any older still-connected client.
  if (client_ && client_.get() != new_client.get())
    client_->close();
  client_ = std::move(new_client);
}
