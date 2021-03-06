AC_PREREQ([2.63])
AC_INIT([ostree], [2013.2], [walters@verbum.org])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wno-portability foreign no-define tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
AC_SYS_LARGEFILE

AC_PROG_CC
AM_PROG_CC_C_O

changequote(,)dnl
if test "x$GCC" = "xyes"; then
  WARN_CFLAGS="-Wall -Wstrict-prototypes -Werror=missing-prototypes \
		-Werror=implicit-function-declaration \
		-Werror=pointer-arith -Werror=init-self -Werror=format=2 \
		-Werror=format-security \
		-Werror=missing-include-dirs \
		-Werror=declaration-after-statement"
fi
changequote([,])dnl
AC_SUBST(WARN_CFLAGS)

# Initialize libtool
LT_PREREQ([2.2.4])
LT_INIT([disable-static])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(BUILDDEP_GIO_UNIX, [gio-unix-2.0 >= 2.34.0])
PKG_CHECK_MODULES(BUILDDEP_LVM2APP, [lvm2app >= 2.2])

AC_ARG_ENABLE(documentation,
              AC_HELP_STRING([--enable-documentation],
                             [build documentation]),,
              enable_documentation=yes)
if test x$enable_triggers_only != xno; then
   enable_documentation=no
fi
if test x$enable_documentation = xyes; then
   AC_PATH_PROG([XSLTPROC], [xsltproc])
   if test x$XSLTPROC = x; then
      AC_MSG_ERROR([xsltproc is required to build documentation])
   fi
fi
AM_CONDITIONAL(BUILD_DOCUMENTATION, test x$enable_documentation = xyes)


AC_CONFIG_FILES([
Makefile
])
AC_OUTPUT

echo "
    Roller Derby $VERSION
    ===============

    documentation: $enable_documentation
"
