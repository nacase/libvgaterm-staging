SUBDIRS = vgaterm

SUBDIRS += python

INCLUDES = $(LIBVGATERM_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libvgaterm-1.0.pc

test_libvgaterm_LDADD = vgaterm/libvgaterm-1.0.la $(LIBVGATERM_LIBS)
test_libvgaterm_SOURCES = test-libvgaterm.c

EXTRA_DIST = \
  libvgaterm.spec.in \
  libvgaterm.spec \
  libvgaterm-1.0.pc.in
