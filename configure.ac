#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([sctjudge], [0.9.1], [lantw44@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

releasedate="2012-07-28"

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# 和預設的 CFLAGS 說再見
test -n "${CFLAGS}" && CFLAGS="-g"


# 偵錯很重要，但使用者應該不需要
AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug],
		[produce debugging information and disable 
		optimization in binary files])],
	[opt_debug=$enableval], [opt_debug=no])

if test -n "${CFLAGS}"; then
  if test x"${opt_debug}" = xyes; then
    CFLAGS="-g"
  else
    CFLAGS="-O2"
  fi
fi

# 靜態連結，如果想做 portable 版本的話
AC_ARG_ENABLE([static],
	[AC_HELP_STRING([--enable-static],
		[statically link all executables])],
	[opt_static=$enableval], [opt_static=no])

AM_CONDITIONAL([STATIC_EXEC], [test x"${opt_static}" = xyes])

# 有些功能是選用的......
AC_ARG_ENABLE([procmon], 
	[AS_HELP_STRING([--enable-procmon], 
		[enable process monitor using Linux /proc filesystem])],
	[opt_procmon=$enableval], [opt_procmon=no])

test x"${opt_procmon}" = xyes && \
		 AC_DEFINE([HAVE_CONF_PROCMON], [1], [Process Monitor])

AC_ARG_ENABLE([cap],
	[AS_HELP_STRING([--enable-cap],
		[using Linux capabilities instead of standard UNIX permission])],
	[opt_cap=$enableval], [opt_cap=no])

test x"${opt_cap}" = xyes && \
		AC_DEFINE([HAVE_CONF_CAP], [1], [Linux Capabilities])


# Checks for libraries.

checkliblist="pthread_create pthread_exit pthread_cancel pthread_join pthread_attr_init pthread_attr_destroy pthread_attr_setdetachstate pthread_mutex_init pthread_mutex_destroy pthread_mutex_lock pthread_mutex_unlock pthread_setcancelstate pthread_setcanceltype"

for i in $checkliblist; do
  AC_CHECK_LIB([pthread], $i, [true], 
	[AC_MSG_ERROR(Your POSIX Thread Library may not be properly installed)])
done
LIBS="$LIBS -lpthread -lrt"


checkliblist="sem_init sem_destroy sem_wait sem_post"
for i in $checkliblist; do
  AC_CHECK_LIB([c], $i, [true], [c_sem_notfound=yes])
  AC_CHECK_LIB([pthread], $i, [true], [pthread_sem_notfound=yes])
  if test x"${c_sem_notfound}" = xyes && test x"${pthread_sem_notfound}" = xyes
  then
    AC_MSG_ERROR(Your semaphore support may be incomplete)
  fi
done


if test x"${opt_cap}" = xyes; then
  checkliblist="cap_init cap_clear_flag cap_set_proc"
  for i in $checkliblist; do
    AC_CHECK_LIB([cap], $i, [true],
		AC_MSG_ERROR(You must install libcap-devel or use --disable-cap))
  done
# Checks for programs
  AC_CHECK_PROG([have_prog_setcap],[setcap],[yes],[no])
  test x"${have_prog_setcap}" = xno && 
    AC_MSG_ERROR(setcap does not exist in your PATH)
  LIBS="$LIBS -lcap"
fi

AM_CONDITIONAL([USING_SETCAP], [test x"${opt_cap}" = xyes])

# 寫入版本資訊
AC_SUBST([PROGRAM_NAME], [AC_PACKAGE_NAME])
AC_SUBST([PROGRAM_VERSION], [AC_PACKAGE_VERSION])
AC_SUBST([PROGRAM_DATE], $releasedate)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h locale.h stdlib.h string.h sys/time.h unistd.h])

if test x"${opt_cap}" = xyes; then
  AC_CHECK_HEADERS([sys/prctl.h sys/capability.h], [], 
	[AC_MSG_ERROR(You must install these files or use --disable-cap)])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime dup2 getpagesize memset setlocale strchr strerror strrchr])


AC_CONFIG_FILES([Makefile src/Makefile src/version.h])
AC_OUTPUT

echo ""
echo "Optional Features:"
echo "(1) Process monitor using Linux /proc filesystem .... $opt_procmon"
echo "(2) Linux capabilities support ...................... $opt_cap"
echo "-------------------------------------------------------------"
echo "Compiling and Linking Options:"
echo "(1) Debugging information ........................... $opt_debug"
echo "(2) Statically linked executable .................... $opt_static"
