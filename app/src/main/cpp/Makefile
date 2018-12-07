all: build
	ninja -C $<

build:
	PKG_CONFIG_PATH=subprojects/hdr2sdr/third-party/install/lib/pkgconfig meson $@
