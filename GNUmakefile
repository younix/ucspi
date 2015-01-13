OS := $(shell uname -s)

# GNU/Linux
ifeq "$(OS)" "Linux"
	CFLAGS_BSD := `pkg-config --cflags libbsd`
	LIBS_BSD := `pkg-config --libs libbsd`
	DEFINES := -DUSE_LIBBSD -D_GNU_SOURCE
	TLS = sslc
endif

# MacOSX
ifeq "$(OS)" "Darwin"
	DEFINES := -D_DARWIN_C_SOURCE
	LIBS_TLS := -lcrypto `pkg-config --libs libtls libssl`
endif

include Makefile
