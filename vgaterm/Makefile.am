# Before making a release, the LTVERSION (libtool version) string
# should be modified.  The string is of the form C:R:A.  This is
# NOT the same as the library package version.
#
# - If interfaces have been changed or added, but binary compatibility
#   has been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (e.g., removed or changed
#   interfaces), change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
#

LTVERSION = 0:0:0

if OS_WIN32
export_symbols = -export-symbols vgaterm.def

install-libtool-import-lib:
	$(INSTALL) .libs/libvgaterm-1.0.dll.a $(DESTDIR)$(libdir)

uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libvgaterm-1.0.dll.a
else

export_symbols = -export-symbols-regex "^[^_].*"

install-libtool-import-lib:
uninstall-libtool-import-lib:
endif


INCLUDES = \
  -I$(top_srcdir) \
  $(PACKAGE_CFLAGS) \
  -DVGATERM_LIBDIR=\""$(libdir)"\" \
  -DVGATERM_PREFIX=\""$(prefix)"\"

vgatermmoduledir = $(libdir)/libvgaterm/1.0

lib_LTLIBRARIES = libvgaterm-1.0.la

vgatermincludedir = $(includedir)/libvgaterm-1.0/vgaterm
vgaterminclude_HEADERS = \
  vgaterm.h

EXTRA_libvgaterm_1_0_la_SOURCES = \
  marshal.list

libvgaterm_1_0_la_SOURCES = \
  vgaterm.c \
  vgaterm.h \
  def_font.h \
  def_palette.h \
  marshal.c \
  marshal.h \
  cbuf.c cbuf.h \
  scrollbuf.c scrollbuf.h \
  vgatext.c vgatext.h \
  vgafont.c vgafont.h \
  emulation.c emulation.h \
  terminal.c terminal.h \
  vgapalette.c vgapalette.h

libvgaterm_1_0_la_LIBADD = $(PACKAGE_LIBS)
libvgaterm_1_0_la_LDFLAGS = -version-info $(LTVERSION) $(export_symbols) -no-undefined

# Generated sources

BUILT_SOURCES = marshal.c marshal.h
CLEANFILES = marshal.c marshal.h

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vga_term_marshal --header --body --internal $< > $@

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vga_term_marshal --header --internal $< > $@

EXTRA_DIST = $(vgaterminclude_HEADERS) \
	vgaterm.def

install-data-local: install-libtool-import-lib

uninstall-local: uninstall-libtool-import-lib
