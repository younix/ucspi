OS := $(shell uname -s)

# GNU/Linux
ifeq ($(OS), Linux)
	CFLAGS_BSD := `pkg-config --cflags libbsd`
	LIBS_BSD := `pkg-config --libs libbsd`
	DEFINES := -DUSE_LIBBSD -D_GNU_SOURCE
endif

# MacOSX
ifeq ($(OS), Darwin)
	DEFINES := -D_DARWIN_C_SOURCE
	LIBS_TLS := `pkg-config --libs libtls`
endif

include Makefile
