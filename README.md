## This is a provisional README file.  I didn't start adding in the good stuff yet.

### If you're wondering why I'm vendoring certain third party libraries:

- [JSON for Modern C++](https://github.com/nlohmann/json) - Because I only need the headers.
  I don't know how to pull that off with just submodules, else I would.

- [Boost](https://www.boost.org) - Because I don't need the whole thing.  The zip file with
  all the headers is well over 200 megabytes.  I only need the regex implementation.

- [Libwebp](https://github.com/webmproject/libwebp) - I only need the includes.

- [Httplib](https://github.com/yhirose/cpp-httplib) - I only need the headers. I made modifications
  to the headers to make it compile with MinGW 6.3.0.\*  I'll add them.

- [Asio](https://think-async.com/Asio) - I made changes\* to asio that allow Discord Messenger
  to start on Windows NT 4 (not run though... more details soon)

- [Websocketpp](https://github.com/zaphoyd/websocketpp) - I made certain changes\* to make it compile
  with MinGW 6.3.0.  The modifications will be documented.

\* -- You can use the vanilla version of these libraries, as long as you don't intend on compiling
   with MinGW.  Visual Studio 2022 will always compile these.

