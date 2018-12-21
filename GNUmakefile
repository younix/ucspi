OS := $(shell uname -s)

# GNU/Linux
ifeq "$(OS)" "Linux"
	CFLAGS += -DUSE_LIBBSD
	CFLAGS += -D_GNU_SOURCE
	CFLAGS += `pkg-config --cflags libbsd`
	LDFLAGS += `pkg-config --libs libbsd`
endif

# MacOSX
ifeq "$(OS)" "Darwin"
	CFLAGS += -D_DARWIN_C_SOURCE
	LDFLAGS += -lcrypto `pkg-config --libs libtls libssl`
endif

include Makefile
