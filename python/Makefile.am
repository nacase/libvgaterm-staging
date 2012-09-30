EXTRA_DIST = vgaterm.defs pyvgaterm.pc.in vgaterm.override cat.py unrealize.py vgaterm-demo.py

if BUILD_PYTHON_MODULES

defsdir = $(datadir)/pygtk/2.0/defs
defs_DATA = vgaterm.defs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pyvgaterm.pc

PYTHON_MODULES = vgatermmodule.la

pythonsiteexecdir = $(pyexecdir)/gtk-2.0
pythonsiteexec_LTLIBRARIES = $(PYTHON_MODULES)

CLEANFILES = vgaterm.c

vgatermmodule_la_SOURCES = vgatermmodule.c
vgatermmodule_la_CPPFLAGS = -I$(top_builddir)/vgaterm $(AM_CPPFLAGS)
vgatermmodule_la_CFLAGS = $(VGATERM_CFLAGS) $(PACKAGE_CFLAGS) $(X_CFLAGS) $(PYTHON_INCLUDES) $(PYGTK_CFLAGS) $(AM_CFLAGS)

# we do this to suport building with -Wl,-z,defs which errors on
# vgatermmodule.so as we cannot include $(PYTHON_LIBS) due to bug 410986.
vgatermmodule_la_LDFLAGS = -module -avoid-version -export-symbols-regex initvgaterm $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
vgatermmodule_la_LIBADD = $(top_builddir)/vgaterm/libvgaterm-1.0.la $(LIBS) $(PYGTK_LIBS) $(VGATERM_LIBS) $(X_LIBS)
nodist_vgatermmodule_la_SOURCES = vgaterm.c

vgaterm.c: vgaterm.defs vgaterm.override

.defs.c:
	$(AM_V_GEN) (cd $(srcdir) \
	 && $(PYGOBJECT_CODEGEN) \
            --register $(PYGTK_DEFSDIR)/pango-types.defs \
            --register $(PYGTK_DEFSDIR)/gdk-types.defs \
            --register $(PYGTK_DEFSDIR)/gtk-types.defs \
            --override $*.override \
            --prefix py$* $*.defs) > gen-$*.c \
        && cp gen-$*.c $*.c \
        && rm -f gen-$*.c
	ln -s .libs/vgatermmodule.so .
endif
