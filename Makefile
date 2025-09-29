# User settings

# -----------------------------
# User-configurable variables
# -----------------------------
DEBUG ?= yes                 # yes = build debug with symbols, no = release
UNICODE ?= yes               # yes = build Unicode, no = ANSI
IS_MINGW_ON_WINDOWS ?= no    # yes if compiling on native Windows with MinGW


# Optional environment overrides
USER_INC_DIRS ?=
USER_DEFINES  ?=
COMMIT_HASH ?= undefined

# Windows version macros
WINVER  ?= 0x0501
WIN32IE ?= 0x0500

# Build directories
BUILD_DIR = build
BIN_DIR   = bin
SRC_DIR   = src

# Target executable
TARGET = $(BIN_DIR)/DiscordMessenger.exe

# -----------------------------
# Toolchain configuration
# -----------------------------
ifeq ($(IS_MINGW_ON_WINDOWS),yes)
	MSYS_PATH    ?= C:/MinGW/msys/1.0

	OPENSSL_DIR  ?= C:/DiscordMessenger/openssl
	LIBWEBP_DIR  ?= C:/DiscordMessenger/libwebp/build
	
	DMCC    ?= gcc
	DMCXX   ?= g++
	DMWR    ?= windres
	DMSTRIP ?= strip
	MKDIR   ?= $(MSYS_PATH)/bin/mkdir.exe
	FIND    ?= $(MSYS_PATH)/bin/find.exe
else
	OPENSSL_DIR  ?= /mnt/c/DiscordMessenger/openssl
	LIBWEBP_DIR  ?= /mnt/c/DiscordMessenger/libwebp/build

	DMPREFIX ?= i686-w64-mingw32
	DMCC     ?= $(DMPREFIX)-gcc
	DMCXX    ?= $(DMPREFIX)-g++
	DMWR     ?= $(DMPREFIX)-windres
	DMSTRIP  ?= $(DMPREFIX)-strip
	MKDIR    ?= mkdir
	FIND     ?= find

	# Extra flags for static linking
	EXTRA_FLAGS=-static-libgcc -static-libstdc++ -Wl,--no-whole-archive
endif

# Print info
$(info Discord Messenger makefile)
$(info Debug: $(DEBUG))
$(info Unicode: $(UNICODE))

# -----------------------------
# Include and library paths
# -----------------------------
OPENSSL_INC_DIR = $(OPENSSL_DIR)/include
OPENSSL_LIB_DIR = $(OPENSSL_DIR)

SYSROOTD=
ifdef SYSROOT
	SYSROOTD = --sysroot=$(SYSROOT)
endif

INC_DIRS = \
	$(USER_INC_DIRS) \
	-I$(OPENSSL_INC_DIR) \
	-Ideps \
	-Ideps/asio \
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

# Optional git commit hash define
ifneq ($(COMMIT_HASH),undefined)
	DEFINES += -DGIT_COMMIT_HASH=$(COMMIT_HASH)
endif

# Unicode defines
# note: USE_IPROGS_REIMPL is defined so that iprogsthreads will use the mwas
# version of certain APIs such as TryEnterCriticalSection
ifeq ($(UNICODE), yes)
	UNICODE_DEF = -DUNICODE -D_UNICODE
else
	UNICODE_DEF =
endif

# Debug/release flags
ifeq ($(DEBUG), yes)
	DEBUG_DEF = -D_DEBUG -g -O0 -fno-omit-frame-pointer
else
	DEBUG_DEF = -DNDEBUG -O2 -fno-omit-frame-pointer
endif

# -----------------------------
# Linker subsystem version
# -----------------------------
XL = -Xlinker
MJSSV = $(XL) --major-subsystem-version $(XL)
MNSSV = $(XL) --minor-subsystem-version $(XL)
MJOSV = $(XL) --major-os-version $(XL)
MNOSV = $(XL) --minor-os-version $(XL)

# Give it a subsystem version of 4.0 and an OS version of 1.0
# (replace with 3.10 if you intend to run on NT 3.1)
SSYSVER = $(MJSSV) 4 $(MNSSV) 0 $(MJOSV) 1 $(MNOSV) 0

# -----------------------------
# Compiler and linker flags
# -----------------------------
CXXFLAGS = \
	$(INC_DIRS) \
	$(DEFINES) \
	-MMD \
	-std=c++11 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-march=i586 \
	$(UNICODE_DEF) \
	$(DEBUG_DEF)

LDFLAGS = \
	$(LIB_DIRS) \
	$(SSYSVER) \
	-mwindows \
	-lmswsock -lwsock32 -lcomctl32 -lgdi32 -luser32 -lole32 -lcrypt32 \
	-lcrypto -lssl \
	$(EXTRA_FLAGS)

# Optional WebP support
ifeq ($(DISABLE_WEBP),1)
else
	LIB_DIRS += -L$(LIBWEBP_DIR)
	LDFLAGS  += -lwebp
endif

# Resource compiler flags
WRFLAGS = -Ihacks

# -----------------------------
# Source files
# -----------------------------
# Auxiliary dependencies
AUX_CXXFILES = \
	$(shell $(FIND) deps/iprogsthreads/src -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/asio/src          -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/mwas/src          -not -path '*/.*' -type f -name '*.cpp') \
	$(shell $(FIND) deps/md5               -not -path '*/.*' -type f -name '*.cpp')

# All source files
CXXFILES := $(shell $(FIND) $(SRC_DIR) -not -path '*/.*' -type f -name '*.cpp') $(AUX_CXXFILES)
RESFILES := $(shell $(FIND) $(SRC_DIR) -not -path '*/.*' -type f -name '*.rc')

# Objects and dependency files
OBJ := $(patsubst %, $(BUILD_DIR)/%, $(CXXFILES:.cpp=.o) $(RESFILES:.rc=.o))
DEP := $(patsubst %, $(BUILD_DIR)/%, $(CXXFILES:.cpp=.d))

# -----------------------------
# Default targets
# -----------------------------
.PHONY: all clean
all: $(TARGET)

clean:
	@echo ">> Cleaning build directory"
	@$(FIND) $(BUILD_DIR) -mindepth 1 -exec rm -rf {} +

# Include dependency files
-include $(DEP)

# -----------------------------
# Directory creation rule
# -----------------------------
# Creates any directory needed for a target
$(BUILD_DIR)/%/:
	@mkdir -p $@

# -----------------------------
# Compilation patterns
# -----------------------------
# Compute object directory for a given source file
OBJ_DIR = $(BUILD_DIR)/$(dir $<)

# Compile C++ files
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)/%/
	@echo ">> Compiling $<"
	@$(DMCXX) $(SYSROOTD) $(CXXFLAGS) -c $< -o $@ -MMD -MF $(BUILD_DIR)/$*.d

# Compile resource files
$(BUILD_DIR)/%.o: %.rc | $(BUILD_DIR)/%/
	@echo ">> Compiling resource $<"
	@$(DMWR) $(WRFLAGS) -i $< -o $@ --use-temp-file

# -----------------------------
# Linking
# -----------------------------
$(TARGET): $(OBJ)
	@echo ">> LINKING $@"
	@$(MKDIR) -p $(dir $@)
	@$(DMCXX) $(SYSROOTD) $^ $(LDFLAGS) -o $@
ifeq ($(DEBUG),no)
	@echo ">> Duplicating binary and stripping symbols"
	@cp $@ $(basename $@)-Symbols.exe
	@$(DMSTRIP) $@
endif