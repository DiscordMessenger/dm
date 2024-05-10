# User settings
DEBUG ?= yes
UNICODE ?= yes
MSYS_PATH ?= C:/MinGW/msys/1.0
OPENSSL_INC_DIR ?= C:/crpa/openssl/include
OPENSSL_LIB_DIR ?= C:/crpa/openssl

USER_INC_DIRS =
USER_DEFINES  =

# WINVER definitions
WINVER  ?= 0x0501
WIN32IE ?= 0x0400

# == Don't change below unless you know what you are doing! ==
BUILD_DIR = build
BIN_DIR = bin
SRC_DIR = src

# Target executable
TARGET = $(BIN_DIR)/DiscordMessenger.exe

# Location of certain utilities.  Because Win32 takes over if you don't
MKDIR = $(MSYS_PATH)/bin/mkdir.exe
FIND  = $(MSYS_PATH)/bin/find.exe
WR = windres

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
	-DASIO_HAS_THREADS            \
	-DASIO_DISABLE_STD_FUTURE     \
	-DASIO_DISABLE_GETADDRINFO    \
	-DASIO_SEPARATE_COMPILATION   \
	-DASIO_IPROGS_THREADS         \
	-D_WEBSOCKETPP_IPROGS_THREAD_ \
	-DMINGW_SPECIFIC_HACKS        \
	-DASIO_DISABLE_WINDOWS_OBJECT_HANDLE

ifeq ($(UNICODE), no)
	UNICODE_DEF =
else
	UNICODE_DEF = -DUNICODE -D_UNICODE
endif

ifeq ($(DEBUG), yes)
	DEBUG_DEF = -D_DEBUG -g -O0
else
	DEBUG_DEF = -DNDEBUG -O2
endif

CXXFLAGS = \
	$(INC_DIRS)    \
	$(DEFINES)     \
	-MMD           \
	-std=c++11     \
	$(UNICODE_DEF) \
	$(DEBUG_DEF)

LDFLAGS = \
	$(LIB_DIRS) \
	-mwindows   \
	-lmswsock   \
	-lws2_32    \
	-lcomctl32  \
	-lgdi32     \
	-luser32    \
	-lole32     \
	-lcrypto    \
	-lssl

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
	@$(CXX) $(OBJ) $(LDFLAGS) -o $@

# NOTE: Using --use-temp-file seems to get rid of some weirdness with MinGW 6.3.0's windres?
$(BUILD_DIR)/%.o: %.rc
	@echo \>\> Compiling resource $<
	@$(MKDIR) -p $(dir $@)
	@$(WR) $(WRFLAGS) -i $< -o $@ --use-temp-file

$(BUILD_DIR)/%.o: %.cpp
	@echo \>\> Compiling $<
	@$(MKDIR) -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@
