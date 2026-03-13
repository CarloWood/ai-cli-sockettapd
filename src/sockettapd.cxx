#include "sys.h"
#include "Sockettapd.h"
#include "STListenSocket.h"
#include "evio/EventLoop.h"
#include "utils/debug_ostream_operators.h"      // Needed to write error to Dout.
#include "utils/AIAlert.h"
#include "debug.h"
#include <sstream>
#include <syslog.h>

int main(int argc, char* argv[])
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()");
  Debug(libcw_do.always_flush_on());

  try
  {
    Sockettapd application(argc, argv);

    // If --foreground is not specified then now we are running as a background process.
    bool const syslog_enabled = !application.foreground();
    struct SyslogGuard
    {
      bool enabled_;
      explicit SyslogGuard(bool enabled) : enabled_(enabled) { if (enabled_) ::openlog("sockettapd", LOG_PID, LOG_DAEMON); }
      ~SyslogGuard() { if (enabled_) ::closelog(); }
    };
    SyslogGuard syslog_guard(syslog_enabled);

    // Check that libcwd is always flushing.
    ASSERT(libcwd::libcw_do.always_flush_is_on());

    std::filesystem::path const socket_path = application.projectdir() / "AAP" / application.socket_name();
    evio::SocketAddress endpoint(socket_path.native());
    Dout(dc::notice, "endpoint = " << endpoint);

    // An object that allows us to write to an ostream
    // in order to write to (the buffer of) the socket.
    evio::OutputStream socket_stream;

    try
    {
      // Create a listen socket.
      auto listen_socket = evio::create<STListenSocket>();
      Dout(dc::notice, "Writing to this log file 7.");
      listen_socket->listen(endpoint);

      // Run the application.
      application.run();
    }
    catch (AIAlert::Error const& error)
    {
      Dout(dc::warning, error);
      if (syslog_enabled)
      {
        std::ostringstream oss;
        oss << error;
        ::syslog(LOG_WARNING, "%s", oss.str().c_str());
      }
    }
  }
  catch (NoError const&)
  {
  }

  Dout(dc::notice, "Leaving main()");
}
