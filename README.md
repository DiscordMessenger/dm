# Discord Messenger for Windows

Discord Messenger is a messenger application designed to be compatible with Discord, while being
backwards compatible with down to Windows 2000 (although support for even older versions has been
attempted).

Its motto: *It's time to ditch MSN and Yahoo.*

**NOTE**: This is beta software, so there may be issues which need to be fixed!

The project is licensed under the MIT license.

## Disclaimer

Using third party clients is against Discord's TOS! Although the risk to get banned is low, the
risk is there! The author of this software is not responsible for the status of your Discord
account.

See https://twitter.com/discord/status/1229357198918197248.

## Discord Server

A Discord server about this client can be joined here: https://discord.gg/cEDjgDbxJj

###### Note, you will need to use an official client to accept invitations currently. This may change in the future.

## Screenshots

![Windows 2000 screenshot](doc/ss_2000.png)
![Windows XP screenshot](doc/ss_xp.png)

## Minimum System Requirements

- Windows NT 3.1, Windows 95, or newer (MinGW version)

- Windows XP SP2 or newer (MSVC version)

- Pentium Pro or Pentium 2 CPU (MinGW version) Pentium 4 CPU (MSVC version)

- 64 MB of RAM, can do lower but might start to hit the page file

## Building

Before you can start the build process, after cloning the project (You should NOT download it as
ZIP, unless you know that you should also download the submodules individually and unzip them in
the correct locations), check out the submodules with the command:
`git submodule update --init`.

Then you can start the build process.

You can build this project in two ways.

### 1. Visual Studio

This method can only support down to Windows XP SP2, but it's easier to get started with.

You must have CMake for Windows installed.

1. Compile Mbed TLS for Win32.

First, clone the repository.
```
wget https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.4/mbedtls-3.6.4.tar.bz2
tar -xvjf mbedtls-3.6.4.tar.bz2
cd mbedtls-3.6.4
```

Then, apply the patches:
```
git apply [DM project root]/mbedtls-patches.diff
```

Finally, run the following commands in a VS Developer Command Prompt:
```
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32 -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DCMAKE_INSTALL_PREFIX="%CD%\..\install"
cmake --build . --config Release --target INSTALL
```

(Note: If you have other cmake.exe's defined, you must use the Visual Studio provided one.

Finally, define the environment variable `MBEDTLS_INSTALL_MSVC` to point to the `install` directory,
located inside your mbedtls checkout directory.

2. Compile libcurl for Win32.

First, download curl 8.11.1.

Note: curl 8.12 introduced a breaking change which [causes libpsl to be required](https://github.com/curl/curl/issues/16486)

```
wget https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.bz2
tar -xvjf curl-8.11.1.tar.bz2
```

Finally, run the following commands in a VS Developer Command Prompt:
```
cmake .. -G "Visual Studio 17 2022" -A Win32 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCURL_USE_MBEDTLS=ON ^
  -DMBEDTLS_INCLUDE_DIR="%MBEDTLS_INSTALL_MSVC%/include" ^
  -DMBEDTLS_LIBRARY="%MBEDTLS_INSTALL_MSVC%/lib/mbedtls.lib" ^
  -DMBEDX509_LIBRARY="%MBEDTLS_INSTALL_MSVC%/lib/mbedx509.lib" ^
  -DMBEDCRYPTO_LIBRARY="%MBEDTLS_INSTALL_MSVC%/lib/mbedcrypto.lib" ^
  -DCURL_DISABLE_FTP=ON ^
  -DCURL_DISABLE_FILE=ON ^
  -DCURL_DISABLE_LDAP=ON ^
  -DCURL_DISABLE_LDAPS=ON ^
  -DCURL_DISABLE_RTSP=ON ^
  -DCURL_DISABLE_DICT=ON ^
  -DCURL_DISABLE_TELNET=ON ^
  -DCURL_DISABLE_TFTP=ON ^
  -DCURL_DISABLE_POP3=ON ^
  -DCURL_DISABLE_IMAP=ON ^
  -DCURL_DISABLE_SMTP=ON ^
  -DCURL_DISABLE_GOPHER=ON ^
  -DCURL_DISABLE_MQTT=ON ^
  -DCURL_DISABLE_SMB=ON ^
  -DCURL_DISABLE_IPFS=ON ^
  -DCURL_TARGET_WINDOWS_VERSION=0x0501 ^
  -DCMAKE_INSTALL_PREFIX="%CD%\..\install"
cmake --build . --config Release --target INSTALL
```

Finally, define the environment variable `CURL_INSTALL_MSVC` to point to the `install` directory,
located inside your curl checkout directory.

If you want to build the 64-bit build, change Win32 to x64 in the configuration commands on both, and define `MBEDTLS_INSTALL_MSVC64` and `CURL_INSTALL_MSVC64`.

### 2. MinGW (on Linux, targeting Windows)

[TODO]


## Features
### Implemented

- Viewing and interacting with servers and direct messages
- Viewing and downloading images and attachments
- Uploading attachments
- Editing messages
- Deleting messages
- Replying to messages
- Typing indicator
- URL hotlinks with untrusted link warning dialog (1)
- Viewing member list in servers (2)
- Viewing pinned messages in server
- Embeds (6)
- Showing profile pictures in DM list
- User notes

### Unimplemented but planned

- Friends list
- Viewing member list in group messages and DMs
- Dark mode on modern systems (3)
- Using an asynchronous HTTP library (4)
- Entering voice channels (5)
- Blocking, closing DMs, removing as friend
- Muting channels
- Changing nickname
- More options in the "Preferences" menu
- Assigning a custom status

### Unplanned Features

- Sending friend requests
- Creating DM channels
- Logging in using QR code (7) (8)
- Logging in using e-mail address and password (7)
- Joining servers (7)

### Note
1. You may need a modern browser to actually access most links.

2. Only the first 100 users. I plan on changing it.

3. Would take a lot of effort, but theoretically it is possible. No, it's not as simple as hooking
   certain APIs.

4. Currently, we are using a synchronous HTTP library, with threads to simulate async behavior.

5. Planned for far in the future.

6. Embeds are incomplete, for example, fields don't function properly.

7. Action is weighted by Discord's anti-spam measures. It could cause the target to get autobanned.

8. Some code already exists, but this feature is unfinished and will probably never be finished.

## Compiling OpenSSL for older Windows versions

You will need to use the later MinGW forks (not the original one as this project wants).
Start by opening `mingw32` (from `mingw-w64`), then cloning the OpenSSL repo. (found at
the following link: https://github.com/DiscordMessenger/openssl.git )

Then, run the `./buildit` command.

To use the final libraries and DLLs when compiling Discord Messenger, use `[OpenSSL repo root]/include`
as your `OPENSSL_INC_DIR`, and `[OpenSSL repo root]` as your OPENSSL_LIB_DIR.

[TODO: Figure out how to avoid this]

[TODO: Maybe remove all scanf uses?]

NOTE: On Windows 2000 and earlier, OpenSSL can't link because it uses `_strtoi64` and
`_strtoui64`. Replace these imports with functions likely to return 0 such as `iswxdigit`
and `isleadbyte` respectively, using a hex editor.

## Running on Windows NT 3.1

**NOTE: You do not need to follow these steps if you don't intend on running
Discord Messenger on Windows NT 3.1.**

Windows NT 3.1 doesn't define certain functions in Kernel32 that MinGW libraries expect
to be there.  It also doesn't come with a copy of `MSVCRT.DLL`.  As such, you must acquire
a copy of `MSVCRT.DLL` (for example, from Windows 95), then patch all of the binaries
(including `msvcrt.dll` but except `DiscordMessenger.exe`), to replace all of the
**uppercase** `KERNEL32.DLL` text to `DIMEKE32.DLL`.  Then, download and compile the
https://github.com/DiscordMessenger/kernel32shim.git repo with MinGW (the one you used
to compile DM) by running `make`.  It should produce a `dimeke32.dll` executable which
you then can place in your installation.

One more thing, you must byte patch DiscordMessenger.exe to report a minimum subsystem
version of 3.10 (as opposed to 4.0).  The version is located at offset 0xC8 or 200
(**check if the bytes match `04 00 00 00`**).  Overwrite it with the following byte
string: `03 00 0A 00`. Then, save.  You must also do this on libgcc_s_dw2-1.dll,
libstdc++-6.dll, and msvcrt.dll.

## Attributions

Discord Messenger is powered by the following external libraries:

- [JSON for Modern C++](https://github.com/nlohmann/json)
- [Boost](https://www.boost.org)
- [Libwebp](https://github.com/webmproject/libwebp)
- [Httplib](https://github.com/yhirose/cpp-httplib)
- [Asio](https://think-async.com/Asio)
- [Websocketpp](https://github.com/zaphoyd/websocketpp)

Although these libraries are vendored, you can replace them with the latest version, and the MSVC
build will keep working.  Adjustments were made to certain libraries to make them compile on MinGW.
See `doc/` for details.
