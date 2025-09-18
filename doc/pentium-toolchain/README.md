## Building a toolchain for Discord Messenger that targets the Pentium processor and Windows NT 3.1 / 95

This process is more involved than just using the standard mingw-w64 distribution that your Linux system comes with.
However, it is necessary because the packages typically don't include support for such arcane versions of Windows.

Chances are low you'll get it compiling inside MinGW/MSYS2, I have personally used WSL 1 for compilation.

**If you use Windows and are trying to use WSL 1 to compile the toolchain, be sure to disable your antivirus, as it'll likely make your build slower.**

[The official mingw-w64 building tutorial](https://sourceforge.net/p/mingw-w64/wiki2/Build%20a%20native%20Windows%2064-bit%20gcc%20from%20Linux%20%28including%20cross-compiler%29/)
is the basis for this build guide.

### Setup

First, you will need to locate the following packages:
- `binutils-2.40`
- `gcc-13.1.0`
- `mingw-w64-v13.0.0`

You can use the official FTP servers or mirrors, both will work, just make sure you don't download from a hijacked mirror!
Also, you can probably use other packages, but I make no guarantees that the diffs will patch properly!

Create the following directory structure:
```
mingw-builds:
	build:
		cross:
			mingw-w64-headers
			mingw-w64
			gcc
			binutils
	install:
		cross
	src:
		gcc
		mingw-w64
		binutils
```

We will not be building the native toolchain because it's not necessary.

Export the directory that contains `mingw-builds` as `START_DIR`. Also for brevity I have defined `DM_PATH` as your Discord Messenger repo checkout path.

Extract:
- `binutils-2.40.tar` to `mingw-builds/src/binutils`
- `gcc-13.1.0.tar` to `mingw-builds/src/gcc`
- `mingw-w64-v13.0.0.tar` to `mingw-builds/src/mingw-w64`

Then, apply the patches:
```bash
cd $START_DIR/mingw-builds/src/gcc
patch -p1 -i $DM_PATH/doc/pentium-toolchain/gcc.diff

cd $START_DIR/mingw-builds/src/mingw-w64
patch -p1 -i $DM_PATH/doc/pentium-toolchain/mingw-w64.diff
```
(replace `$DM_PATH` with your Discord Messenger repo checkout path)

### Building the cross-compiler

#### Binutils

First, you will need to compile binutils.  Binutils can be used vanilla, they don't require special patches.

```
cd $START_DIR/mingw-builds/build/cross/binutils
../../../src/binutils/configure --prefix=$START_DIR/mingw-builds/install/cross --target=i686-w64-mingw32 --disable-multilib

make -j$(nproc)
make install
```

#### Mingw-w64 CRT headers

You will need to install the Mingw-w64 CRT headers.  These also do not require special patches.

```
cd $START_DIR/mingw-builds/build/cross/mingw-w64-headers
../../../src/mingw-w64/mingw-w64-headers/configure --host=i686-w64-mingw32 --prefix=$START_DIR/mingw-builds/install/cross/i686-w64-mingw32 --with-default-win32-winnt=0x0500 --with-default-msvcrt=crtdll

make install
```

### GCC

**Make sure you have applied gcc.diff from this directory onto `$START_DIR/mingw-builds/src/gcc` before configuring.**

To configure GCC, you will need to run the following commands:

```
cd $START_DIR/mingw-builds/build/cross/gcc
../../../src/gcc/configure --prefix=$START_DIR/mingw-builds/install/cross --target=i686-w64-mingw32 --disable-multilib --enable-languages=c,c++ --with-arch=pentium --with-tune=pentium --disable-libgcov --disable-libgomp
```

Build the compiler, which will compile the Mingw-w64 CRT as well as all auxiliary libraries.

```
make all-gcc -j$(nproc)
make install-gcc
```

Note: We are not building the libraries just yet, because we need the Mingw-w64 CRT built first.

#### Mingw-w64 CRT

Now that the core compiler is built, you need to build the Mingw-w64 C runtime.

**Make sure you have applied mingw-w64.diff from this directory onto `$START_DIR/mingw-builds/src/mingw-w64` before configuring.**

Configure and build Mingw-w64:
```
cd $START_DIR/mingw-builds/build/cross/mingw-w64
../../../src/mingw-w64/configure --host=i686-w64-mingw32 --prefix=$START_DIR/mingw-builds/install/cross/i686-w64-mingw32 --with-default-win32-winnt=0x0500 --with-default-msvcrt=crtdll --disable-wchar

make
make install
```

**IMPORTANT NOTE**: There is a race condition as of v13.0.0 which prevents parallel compilation from being successful. Unfortunately, that means you will need to wait a long time (about 30 minutes) for it to finish.

#### GCC Libraries (libgcc, libstdc++)

Finally, you need to build the libraries.
```
cd $START_DIR/mingw-builds/build/cross/gcc

make -j$(nproc)
make install
```

### Fin

With this, your toolchain is now complete!  Try compiling Discord Messenger with it:

```
make DEBUG=no UNICODE=no -j$(nproc) DMPREFIX=$START_DIR/mingw-builds/install/cross/bin/i686-w64-mingw32
```

Enjoy!
