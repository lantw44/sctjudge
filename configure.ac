#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([sctjudge], [1.0-rc2], [lantw44@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([Makefile sctjudge.spec src/Makefile src/version.h])

releasedate="2012-11-13"
rpmversion="1.0"

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# 寫入版本資訊
AC_SUBST([PROGRAM_NAME], [AC_PACKAGE_NAME])
AC_SUBST([PROGRAM_VERSION], [AC_PACKAGE_VERSION])
AC_SUBST([PROGRAM_RPMVERSION], $rpmversion)
AC_SUBST([PROGRAM_DATE], $releasedate)

# 提早離開 configure 的途徑
AC_ARG_ENABLE([check],
	[AS_HELP_STRING([--disable-check],
		[skip all checks])],
	[opt_check=$enableval], [opt_check=yes])

if test x"${opt_check}" = xno; then \
  AC_OUTPUT
  echo "WARNING: ALL CHECKS ARE SKIPPED!"
  echo "WARNING: Generated Makefile(s) cannot be used to build the package!"
  exit 0
fi

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
		[enable process monitor using Linux proc filesystem])],
	[opt_procmon=$enableval], [opt_procmon=no])

test x"${opt_procmon}" = xyes && \
		 AC_DEFINE([HAVE_CONF_PROCMON], [1], [Process Monitor])

AC_ARG_ENABLE([cap],
	[AS_HELP_STRING([--enable-cap],
		[using Linux capabilities instead of standard UNIX permission])],
	[opt_cap=$enableval], [opt_cap=no])

test x"${opt_cap}" = xyes && \
		AC_DEFINE([HAVE_CONF_CAP], [1], [Linux Capabilities])

# 可以設定 /dev/null 名稱
AC_ARG_WITH([null], 
		AS_HELP_STRING([--with-null=NULL],
			[set the path of the data sink (default: /dev/null)]),
		[with_null=$withval], [with_null=/dev/null])

AC_CHECK_FILE([${with_null}],
		[AC_DEFINE_UNQUOTED([NULL_DEVICE], ["${with_null}"],
			[Path of the data sink])],
		[AC_MSG_ERROR([${with_null} does not exist])])

# 可以設定 proc filesystem 在哪裡
AC_ARG_WITH([proc], 
		AS_HELP_STRING([--with-proc=PROC],
			[set the location of your Linux proc file system]),
		[with_proc=$withval], [with_proc=/proc])

if test x"${opt_procmon}" = xyes; then \
	AC_CHECK_FILES([${with_proc}/stat ${with_proc}/$$/stat ${with_proc}/$$/statm ${with_proc}/$$/status],
		[AC_DEFINE_UNQUOTED([PROC_PATH], ["${with_proc}"], 
			[Location of Linux proc file system])], 
		[AC_MSG_ERROR([Linux proc file system support is incomplete])])
fi

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
AC_CHECK_FUNCS([clock_gettime dup2 memset setlocale strchr strerror strrchr getuid getpid setpgid wait4 fcntl fchown open close setrlimit chmod unlink],
		[], AC_MSG_ERROR(This program requires all these functions!))

AC_CHECK_FUNCS([getpwnam getgrnam getpwuid getgrgid],
		opt_ugidname=yes, opt_ugidname=no)

test x"${opt_ugidname}" = xyes && \
		 AC_DEFINE([HAVE_CONF_UGIDNAME], [1], 
		[Conversion between user or group ID and name])

AC_OUTPUT

confmsgsave="config.msg"

echo "" | tee "$confmsgsave"
echo "Optional Features:" | tee -a "$confmsgsave"
echo "(1) Conversion between user or group ID and name .... $opt_ugidname" | tee -a "$confmsgsave"
echo "(2) Process monitor using Linux proc filesystem ..... $opt_procmon" | tee -a "$confmsgsave"
echo "(3) Linux capabilities support ...................... $opt_cap" | tee -a "$confmsgsave"
echo "-------------------------------------------------------------" | tee -a "$confmsgsave"
echo "Compiling and Linking Options:" | tee -a "$confmsgsave"
echo "(1) Debugging information ........................... $opt_debug" | tee -a "$confmsgsave"
echo "(2) Statically linked executable .................... $opt_static" | tee -a "$confmsgsave"
echo "-------------------------------------------------------------" | tee -a "$confmsgsave"
echo "Strings:" | tee -a "$confmsgsave"
echo "(1) Linux proc filesystem path ...................... $with_proc" | tee -a "$confmsgsave"
echo "(2) Data sink path .................................. $with_null" | tee -a "$confmsgsave"
echo "The above messages has been written to $confmsgsave"
