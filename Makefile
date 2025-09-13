# User settings

# Print debugging information and keep debugging symbols
DEBUG ?= yes

# Compile the Unicode version or the ANSI version
UNICODE ?= yes

# Define IS_MINGW_ON_WINDOWS=yes if you're compiling on Windows
IS_MINGW_ON_WINDOWS ?= no

# NOTE: These defaults only apply on iProgramInCpp's system!
# You must specify something else on your end by either `export`-ing these
# environment variables, or by specifying them in the `make` command line.
ifeq ($(IS_MINGW_ON_WINDOWS),yes)
	# OpenSSL install directory
	OPENSSL_DIR ?= C:/DiscordMessenger/openssl
	MSYS_PATH   ?= C:/MinGW/msys/1.0
	
	# Toolchain
	DMCC  ?= gcc
	DMCXX ?= g++
	DMWR  ?= windres
	MKDIR ?= $(MSYS_PATH)/bin/mkdir.exe
	FIND  ?= $(MSYS_PATH)/bin/find.exe
else
	# OpenSSL install directory
	OPENSSL_DIR ?= /mnt/c/DiscordMessenger/openssl
	
	# Toolchain
	DMCC  ?= i686-w64-mingw32-gcc
	DMCXX ?= i686-w64-mingw32-g++
	DMWR  ?= i686-w64-mingw32-windres
	MKDIR ?= mkdir
	FIND  ?= find
	
	# Extra flags if you need them
	EXTRA_FLAGS=-static-libgcc -static-libstdc++ -Wl,--no-whole-archive
endif

# Print info
$(info Discord Messenger makefile)
$(info Debug: $(DEBUG))
$(info Unicode: $(UNICODE))

USER_INC_DIRS ?=
USER_DEFINES  ?=

# WINVER definitions
WINVER  ?= 0x0501
WIN32IE ?= 0x0500

# == Don't change below unless you know what you are doing! ==
BUILD_DIR = build
BIN_DIR = bin
SRC_DIR = src

OPENSSL_INC_DIR = $(OPENSSL_DIR)/include
OPENSSL_LIB_DIR = $(OPENSSL_DIR)

# Target executable
TARGET = $(BIN_DIR)/DiscordMessenger.exe

# Location of certain utilities.  Because Win32 takes over if you don't
ifeq ($(USE_MINGW_FIND),yes)
else
	MKDIR ?= mkdir
	FIND  ?= find
endif

SYSROOTD=
ifdef SYSROOT
	SYSROOTD=--sysroot=$(SYSROOT)
endif

INC_DIRS = \
	$(USER_INC_DIRS) \
	-I$(OPENSSL_INC_DIR)         \
	-Ideps                       \
	-Ideps/asio                  \
	-Ideps/iprogsthreads/include \
	-Ideps/mwas/include

LIB_DIRS = \
	-L$(OPENSSL_LIB_DIR)

DEFINES = \
	-DWINVER=$(WINVER)            \
	-D_WIN32_WINNT=$(WINVER)      \
	-D_WIN32_IE=$(WIN32IE)        \
	-DASIO_STANDALONE             \
	-DASIO_DISABLE_IOCP           \
	-DASIO_HAS_THREADS            \
	-DASIO_DISABLE_STD_FUTURE     \
	-DASIO_DISABLE_GETADDRINFO    \
	-DASIO_SEPARATE_COMPILATION   \
	-DASIO_IPROGS_THREADS         \
	-D_WEBSOCKETPP_IPROGS_THREAD_ \
	-DMINGW_SPECIFIC_HACKS        \
	-DDISCORD_MESSENGER           \
	-DUSE_IPROGS_REIMPL           \
	-DASIO_DISABLE_WINDOWS_OBJECT_HANDLE \
	$(USER_DEFINES)

COMMIT_HASH ?= undefined
ifneq ($(COMMIT_HASH),undefined)
	DEFINES += -DGIT_COMMIT_HASH=$(COMMIT_HASH)
endif

#note: USE_IPROGS_REIMPL is defined so that iprogsthreads will use the mwas
#version of certain APIs such as TryEnterCriticalSection

ifeq ($(UNICODE), no)
	UNICODE_DEF =
else
	UNICODE_DEF = -DUNICODE -D_UNICODE
endif

ifeq ($(DEBUG), yes)
	DEBUG_DEF = -D_DEBUG -g -O0 -fno-omit-frame-pointer
else
	DEBUG_DEF = -DNDEBUG -O2 -fno-omit-frame-pointer
endif

XL = -Xlinker
MJSSV = $(XL) --major-subsystem-version $(XL)
MNSSV = $(XL) --minor-subsystem-version $(XL)
MJOSV = $(XL) --major-os-version $(XL)
MNOSV = $(XL) --minor-os-version $(XL)

# Give it a subsystem version of 4.0 and an OS version of 1.0
# (replace with 3.10 if you intend to run on NT 3.1)
SSYSVER = $(MJSSV) 4 $(MNSSV) 0 $(MJOSV) 1 $(MNOSV) 0

CXXFLAGS = \
	$(SYSROOTD)    \
	$(INC_DIRS)    \
	$(DEFINES)     \
	-MMD           \
	-std=c++11     \
	-mno-mmx       \
	-mno-sse       \
	-mno-sse2      \
	$(UNICODE_DEF) \
	$(DEBUG_DEF)

LDFLAGS = \
	$(SYSROOTD) \
	$(LIB_DIRS) \
	$(SSYSVER)  \
	-mwindows   \
	-lmswsock   \
	-lwsock32   \
	-lcomctl32  \
	-lgdi32     \
	-luser32    \
	-lole32     \
	-lcrypt32   \
	-lcrypto    \
	-lssl       \
	$(EXTRA_FLAGS)

WRFLAGS = \
	-Ihacks

AUX_CXXFILES = \
	$(shell $(FIND) deps/iprogsthreads/src -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/asio/src          -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/mwas/src          -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/md5               -not -path '*/.*' -type f -name '*.cpp')

# Use find to glob all *.cpp files in the directory and extract the object names.
override CXXFILES := $(shell $(FIND) $(SRC_DIR) -not -path '*/.*' -type f -name '*.cpp') $(AUX_CXXFILES)
override RESFILES := $(shell $(FIND) $(SRC_DIR) -not -path '*/.*' -type f -name '*.rc')
override OBJ := $(patsubst %, $(BUILD_DIR)/%, $(CXXFILES:.cpp=.o) $(RESFILES:.rc=.o))
override DEP := $(patsubst %, $(BUILD_DIR)/%, $(CXXFILES:.cpp=.d))

.PHONY: all
all: all2
all2: $(TARGET)

clean:
	rm -rf $(BUILD_DIR)/*

-include $(DEP)

$(TARGET): $(OBJ)
	@echo \>\> LINKING $@
	@$(MKDIR) -p $(dir $@)
	@$(DMCXX) $(OBJ) $(LDFLAGS) -o $@

# NOTE: Using --use-temp-file seems to get rid of some weirdness with MinGW 6.3.0's windres?
$(BUILD_DIR)/%.o: %.rc
	@echo \>\> Compiling resource $<
	@$(MKDIR) -p $(dir $@)
	@$(DMWR) $(WRFLAGS) -i $< -o $@ --use-temp-file

$(BUILD_DIR)/%.o: %.cpp
	@echo \>\> Compiling $<
	@$(MKDIR) -p $(dir $@)
	@$(DMCXX) $(CXXFLAGS) -c $< -o $@
