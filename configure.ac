dnl Process this file with autoconf
dnl Versions are denoted using a standard triplet of integers: 
dnl MAJOR.MINOR.PATCH. 
dnl The basic intent is that MAJOR versions are incompatible, large-scale
dnl upgrades of the API. MINOR versions retain source and binary compatibility
dnl with older minor versions, and changes in the PATCH level are perfectly
dnl compatible, forwards and backwards.
AC_INIT(libvgaterm, 1.0.0)
AC_PREREQ(2.5)
AC_CONFIG_SRCDIR(vgaterm/vgaterm.h)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(1.8)

dnl Check for toolchain and install components
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_LIBTOOL
AC_LIBTOOL_WIN32_DLL

dnl Check for standard headers:
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([stdlib.h stdio.h])

dnl Check for libraries:
AC_CHECK_LIB(util)

dnl Check for functions:
dnl AC_CHECK_FUNCS([])

pkg_modules="gtk+-2.0 >= 2.8 gthread-2.0 >= 2.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])

#AC_CHECK_PROG(SETPCI, lspci, lspci, [])

AM_CONDITIONAL(OS_WIN32, test "$native_win32" = yes)


#########################
## Python bindings
#########################
BUILD_PYTHON=true
AC_ARG_ENABLE(python, [AS_HELP_STRING(--enable-python,Build python bindings [default=yes])],[
if test x"$enableval" != xno ; then
	BUILD_PYTHON=true
else
	BUILD_PYTHON=false
fi],BUILD_PYTHON=true)

if $BUILD_PYTHON ; then
        AM_PATH_PYTHON
        if test -z "$PYTHON" ; then
                BUILD_PYTHON=false
        fi
fi

if $BUILD_PYTHON ; then
        AC_MSG_CHECKING(for python >= 2.2)
        prog="
import sys, string
minver = (2,2,0,'final',0)
if sys.version_info < minver:
        sys.exit(1)
sys.exit(0)
"
        if $PYTHON -c "$prog" 1>&AC_FD_CC 2>&AC_FD_CC
        then
                AC_MSG_RESULT([okay])
        else
                BUILD_PYTHON=false
                AC_MSG_RESULT([too old])
        fi
fi

if $BUILD_PYTHON ; then
        AM_CHECK_PYTHON_HEADERS(,BUILD_PYTHON=false)
fi

if $BUILD_PYTHON ; then
        AC_MSG_CHECKING([for pygtk-2.0])
        if pkg-config --exists pygtk-2.0 ; then
                AC_MSG_RESULT([found])
                PKG_CHECK_MODULES(PYGTK,[pygtk-2.0])

                AC_PATH_PROG(PYGOBJECT_CODEGEN, pygobject-codegen-2.0, no)
                if test "x$PYGOBJECT_CODEGEN" = xno; then
                  AC_MSG_ERROR(could not find pygobject-codegen-2.0 script)
                fi

                AC_MSG_CHECKING(for pygtk defs)
                PYGTK_DEFSDIR=`$PKG_CONFIG --variable=defsdir pygtk-2.0`
                AC_SUBST(PYGTK_DEFSDIR)
                AC_MSG_RESULT($PYGTK_DEFSDIR)
        else
                AC_MSG_RESULT([not found])
                BUILD_PYTHON=false
        fi
fi

if ! $BUILD_PYTHON ; then
        AC_MSG_RESULT([Python bindings disabled.])
fi

AC_SUBST(PYTHONREV)
AC_SUBST(PYTHONMODULES)

AM_CONDITIONAL(BUILD_PYTHON_MODULES, $BUILD_PYTHON)

#########################

# glib marshal stuff
AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])


AM_CONFIG_HEADER(config.h)
AC_OUTPUT(Makefile python/Makefile python/pyvgaterm.pc vgaterm/Makefile libvgaterm.spec libvgaterm-1.0.pc)
