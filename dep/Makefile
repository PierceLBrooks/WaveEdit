
LOCAL = $(PWD)

include ../Makefile-arch.inc
ifeq ($(ARCH),mac)
	export CFLAGS = \
		-mmacosx-version-min=10.7
	export CXXFLAGS = $(CFLAGS)
	export LDFLAGS = $(CFLAGS)
endif

ifndef sdl2
sdl2 = SDL2-2.0.7
endif
ifndef libsndfile
libsndfile = libsndfile-1.2.2
endif
ifndef libsamplerate
libsamplerate = libsamplerate-0.2.2
endif

# This instance of make should be serialized, but -j flags are passed down to each recursive Makefile
.NOTPARALLEL:

all: $(sdl2) $(libsndfile) $(libsamplerate)

$(sdl2):
	curl -L --insecure --verbose --output "$@.tar.gz" "https://www.libsdl.org/release/$@.tar.gz"
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)" --enable-input-tslib=no
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsndfile):
	curl -L --insecure --verbose --output "$@.tar.xz" "https://github.com/libsndfile/libsndfile/releases/download/1.2.2/$@.tar.xz"
	tar xf $@.tar.xz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsamplerate):
	curl -L --insecure --verbose --output "$@.tar.xz" "https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/$@.tar.xz"
	tar xf $@.tar.xz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

clean:
	git clean -fdx