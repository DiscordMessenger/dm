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

![Windows 11 screenshot](doc/ss_11.png)
![Windows XP screenshot](doc/ss_xp.png)
![Windows 95 screenshot](doc/ss_95.png)
![Windows NT 3.1 screenshot](doc/ss_nt31.png)

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

1. Compile OpenSSL for Win32, or find a distribution of OpenSSL 3.X.

You can acquire OpenSSL for Win32 from the following website if you don't want to bother with
compiling it:
https://slproweb.com/products/Win32OpenSSL.html

(Note: Do not download the "Light" versions, as they only contain the DLLs.)

(Note: Download Win64 if you want to compile for x64, Win32 if you want to compile for Win32).

2. Add an entry to your user/system environment variables called `OPENSSL_INSTALL`. (replace with
`OPENSSL_INSTALL64` everywhere if you are compiling for 64-bit)

Set its value to the place where your OpenSSL distribution is located.

3. If you want to use a later version of libwebp, acquire libwebp from the following web site:
https://developers.google.com/speed/webp/download.  Extract the archive and place "libwebp.lib" in
`vs/libs`.

4. Open the Visual Studio solution `vs/DiscordMessenger.sln`.

5. Click the big play button.  (Both x86 and x64 targets are supported.)

6. Enjoy!

### 2. MinGW (on Linux, targeting Windows)

(Note: x64 compilation with MinGW is currently not supported)

**(NOTE: The versions of MinGW your package manager(s) provide(s) may not target your desired platform!
If you want Pentium 1 support and/or native Windows 95/NT 3.x support, see: [Pentium Toolchain Build Guide](doc/pentium-toolchain/README.md))**

1. Acquire mingw-w64:
```
sudo apt install mingw-w64 gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix
```

2. Set `OPENSSL_INC_DIR` and `OPENSSL_LIB_DIR` in your environment variables to your OpenSSL
include and library directories.  If you want to remember the paths, edit the Makefile to use those
as your defaults, or `export` them (but make sure to not check in your change when sending a PR!)

3. Use the following command line:
```
make DEBUG=no UNICODE=[no|yes] [-j (your core count)]
```

The finished binary will be placed in `./bin/DiscordMessenger.exe`. Enjoy!

### 3. MinGW (old Windows method)

(Note: x64 compilation with MinGW is currently not supported)

1. Acquire MinGW-6.3.0.  This is the last version of the original Minimalist GNU for Windows.

NOTE: You might be able to use Mingw-w64 with 32-bit mode, but you might run into trouble running the
final product on anything newer than XP.

2. Using the MinGW Installation Manager, install or ensure that the following packages are installed:
	- mingw32-base
	- mingw32-binutils
	- mingw32-gcc
	- mingw32-gcc-core-deps
	- mingw32-gcc-g++
	- mingw32-libatomic
	- mingw32-libgcc
	- mingw32-w32api
	- msys-base
	- msys-bash
	- msys-core
	- msys-make

3. Ensure that both the MinGW `bin/` AND msys `bin/` directories are in your `PATH`.

4. Set `OPENSSL_DIR` in your environment variables to your OpenSSL library directory.

If you wish to use Shining Light Productions' distribution of OpenSSL-Win32, copy the
`%OPENSSL_INSTALL%/include` and `%OPENSSL_INSTALL%/lib/MinGW` directories to a new folder
that you assign as `OPENSSL_DIR`, then make sure that your MinGW libraries are actually
in the root of that new folder!

If you want compatibility on Windows versions which don't support the Microsoft Visual Studio 2015
runtimes (VCRUNTIME140.DLL), then you will need to compile OpenSSL yourself.  See the section on
[Compiling OpenSSL for older Windows versions](#compiling-openssl-for-older-windows-versions)
section.

5. Run the `make IS_MINGW_ON_WINDOWS=yes` command.

6. Enjoy!

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

You will need to use the `mingw-w64` (not the original one as this project wanted once upon a time).
Start by cloning the OpenSSL repo found at the following link: https://github.com/DiscordMessenger/openssl.git.

Then, run the `./buildit` command.

To use the final libraries and DLLs when compiling Discord Messenger, use `[OpenSSL repo root]` as your `OPENSSL_DIR`.

## Running on Windows NT 3.x and Windows 9x

**NOTE: You do not need to follow these steps if you don't intend on running
Discord Messenger on these versions of Windows.**

You will need to use the mingw-w64 Pentium toolchain described in the [Pentium Toolchain Build Guide](doc/pentium-toolchain/README.md)),
which additionally provides patches for compatibility with Windows NT 3.x / 9x.

One more thing, you must byte patch DiscordMessenger.exe to report a minimum subsystem version of
3.10 (as opposed to 4.0).  The subsystem version is typically located at offset 0xC8 or 200
(**make sure to check if the bytes match `04 00 00 00`**).  Overwrite it with the following byte
string: `03 00 0A 00`. Then, save.

## Short File Names

If you are planning to run Discord Messenger from a FAT partition on a Windows OS that does not support
LFNs (Windows NT 3.1, 3.5, and Windows 95 betas), then you will need to edit the final executable and its
DLLs to use SFN versions thereof.

- `libcrypto-3.dll` -> `libcrypt.dll`
- `libssl-3.dll` -> `libssl.dll`

You can use a hex editor for this purpose or you can use CFF Explorer.  These changes must be applied to
`DiscordMessenger.exe` *and* `libssl-3.dll`. Also, make sure to rename the libcrypto and libssl DLLs.

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
