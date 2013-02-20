#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([sctjudge], [1.0], [lantw44@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/SctMain.c])
AC_CONFIG_HEADERS([src/SctConfig.h])
AC_CONFIG_FILES([
	Makefile 
	sctjudge.spec 
	l4basic/l4arg/Makefile
	l4basic/l4bds/Makefile
	l4basic/l4darr/Makefile
	src/Makefile 
	src/SctVersion.h
	])

releasedate="2013-01-27"
rpmversion="1.0"





# 寫入版本資訊
AC_SUBST([PROGRAM_NAME], [AC_PACKAGE_NAME])
AC_SUBST([PROGRAM_VERSION], [AC_PACKAGE_VERSION])
AC_SUBST([PROGRAM_RPMVERSION], $rpmversion)
AC_SUBST([PROGRAM_DATE], $releasedate)




# 提早離開 configure 的途徑
AC_ARG_ENABLE([check],
	[AS_HELP_STRING([--disable-check], [skip all checks])],
	[opt_check=$enableval], [opt_check=yes])

if test x"${opt_check}" = xno; then \
  AC_OUTPUT
  echo "WARNING: ALL CHECKS ARE SKIPPED!"
  echo "WARNING: Generated Makefile(s) cannot be used to build the package!"
  exit 0
fi




# 偵測作業系統類型
AC_CANONICAL_HOST
AC_CANONICAL_BUILD




# 根據作業系統修改預設選項
case "$build_os" in
	*linux*)
		opt_procmon=guess
		opt_cap=guess
		;;
	*freebsd*)
		opt_procmon=no
		opt_cap=no
		;;
	*)
		opt_procmon=no
		opt_cap=no
		;;
esac




# 偵錯很重要，但使用者應該不需要
AC_ARG_ENABLE([optimized],
	[AS_HELP_STRING([--disable-optimized], [disable compiler optimization])],
	[opt_optimized=$enableval], [opt_optimized=yes])

if test x"${opt_optimized}" = xno; then
	if test -z "${CFLAGS}"; then
		CFLAGS="-g"
	else
		CFLAGS="${CFLAGS//-O2/}"
	fi
fi



# 偵測必要的程式
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O




# 寫入編譯參數
AC_SUBST([AUTOTOOL_CC], ["$CC"])
AC_SUBST([AUTOTOOL_CFLAGS], ["$CFLAGS"])



# 靜態連結，如果想做 portable 版本的話
AC_ARG_ENABLE([static],
	[AS_HELP_STRING([--enable-static],
		[statically link all executables])],
	[opt_static=$enableval], [opt_static=no])

AM_CONDITIONAL([STATIC_EXEC], [test x"${opt_static}" = xyes])




# 選用的功能
AC_ARG_ENABLE([procmon], 
	[AS_HELP_STRING([--enable-procmon=METHOD], 
		[enable process monitor (METHOD=linux/freebsd) 
		(LINUX=Linux /proc filesystem)
		(FREEBSD=FreeBSD sysctl(3))])],
	[opt_procmon=$enableval], [true])

AC_ARG_ENABLE([cap],
	[AS_HELP_STRING([--enable-cap],
		[use Linux capabilities instead of standard UNIX permission 
		(default: YES if building for Linux)])],
	[opt_cap=$enableval], [true])




# 檢查必要的函式庫
AC_CHECK_LIB([pthread], [pthread_create], [LIBS="$LIBS -lpthread -lrt"], 
		[test_nort=yes], [-lrt])

if test x"$test_nort" = xyes; then 
	AC_CHECK_LIB([pthread], [pthread_create], [LIBS="$LIBS -lpthread"],
		[AC_MSG_ERROR([POSIX Thread Library is required])])
fi

AC_CHECK_LIB([c], [sem_timedwait], [true],
	[AC_MSG_ERROR([Semaphore support is required])], ["$LIBS"])


# Checks for header files.
AC_CHECK_HEADERS([fcntl.h locale.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK

test x"${ac_cv_func_fork_works}" = xyes || \
	AC_MSG_ERROR(This program requires a working fork function.)

AC_FUNC_MALLOC
AC_FUNC_REALLOC

for f in \
	clock_gettime dup2 memset setlocale strchr strerror strrchr getuid \
	getpid setpgid wait4 fcntl fchown open close setrlimit chmod unlink
do 
	AC_CHECK_FUNC([$f],
		[true], AC_MSG_ERROR(This program requires all these functions!))
done

for f in getpwnam getgrnam getpwuid getgrgid 
do 
	AC_CHECK_FUNC([$f], [opt_ugidname=yes], [opt_ugidname=no])
	test x"${opt_ugidname}" = xno && break
done

test x"${opt_ugidname}" = xyes && \
		 AC_DEFINE([HAVE_CONF_UGIDNAME], [1], 
		[Conversion between user or group ID and name])





# 可以設定 /dev/null 名稱
AC_ARG_WITH([null], 
		AS_HELP_STRING([--with-null=NULL],
			[set the path of the data sink (default: /dev/null)]),
		[with_null=$withval], [with_null=/dev/null])

if test "$build" = "$host"; then \
	AC_CHECK_FILE([${with_null}], [true],
			[AC_MSG_ERROR([${with_null} does not exist])])
else
	echo "Cross compiling: Data sink existence check is SKIPPED!"
fi

AC_DEFINE_UNQUOTED([WITH_NULL], ["${with_null}"], [Path of the data sink])






# 判斷是否可用 procmon
# 可以設定 proc filesystem 在哪裡
AC_ARG_WITH([linuxproc], 
		AS_HELP_STRING([--with-linuxproc=PROC],
			[set the location of your Linux proc file system (default: /proc)]),
		[with_linuxproc=$withval], [with_linuxproc=/proc])

if test x"${opt_procmon}" = xyes; then
	case "$build_os" in
		*linux*)
			opt_procmon=linux
			;;
		*freebsd*)
			opt_procmon=freebsd
			;;
		*)
			;;
	esac
fi

case "${opt_procmon}" in
	linux|freebsd|yes|no|guess)
		;;
	*)
		AC_MSG_ERROR([--enable-procmon="${opt_procmon}" is invalid])
		;;
esac
	

if test x"${opt_procmon}" = xlinux || test x"${opt_procmon}" = xguess; then 
	if test "$build" = "$host"; then 
		for f in \
			"${with_linuxproc}/stat"      \
			"${with_linuxproc}/$$/stat"   \
			"${with_linuxproc}/$$/statm"  \
			"${with_linuxproc}/$$/status" 
		do 
			AC_CHECK_FILE([$f], [procmon_found=yes], [procmon_found=no])
			test x"${procmon_found}" = xno && break
		done
		if test x"${procmon_found}" = xyes; then 
			opt_procmon=linux
		else
			if test x"${opt_procmon}" = xlinux; then
				AC_MSG_ERROR([Linux proc file system support is incomplete])
			fi
			opt_procmon=no
		fi
	else
		echo "Cross compiling: Linux proc file system support check is SKIPPED!"
		opt_procmon=linux
	fi
fi


if test x"${opt_procmon}" = xfreebsd || test x"${opt_procmon}" = xguess; then 
	if test x"${opt_procmon}" = xfreebsd; then
		AC_MSG_ERROR([FreeBSD process monitor is not implemented])
	fi
fi



if test x"${opt_procmon}" = xlinux; then
	AC_DEFINE([HAVE_CONF_PROCMON_LINUX], [1], [Process Monitor (Linux)])
	AC_DEFINE_UNQUOTED([WITH_LINUXPROC], ["${with_linuxproc}"], 
			[Location of Linux proc file system])
fi



if test x"${opt_cap}" = xyes || test x"${opt_cap}" = xguess; then
	AC_CHECK_LIB([cap], [cap_set_proc], [cap_found_1=yes], [cap_found_1=no])
	AC_CHECK_PROG([cap_found_2], [setcap], [yes], [no])
	AC_CHECK_HEADERS([sys/prctl.h sys/capability.h], [cap_found_3=yes], 
		[cap_found_3=no])

	if test x"${cap_found_1}" = xyes && test x"${cap_found_2}" = xyes && \
		test x"${cap_found_3}" = xyes; then
		opt_cap=yes
	else
		if test x"${opt_cap}" = xyes; then 
			AC_MSG_ERROR([You must install Linux capabilities library (libcap) 
					or use --disable-cap])
		fi
		opt_cap=no
	fi
fi

if test x"${opt_cap}" = xyes; then 
	LIBS="$LIBS -lcap"
	AC_DEFINE([HAVE_CONF_CAP], [1], [Linux Capabilities])
fi
AM_CONDITIONAL([USING_SETCAP], [test x"${opt_cap}" = xyes])




AC_OUTPUT

confmsgsave="config.msg"

echo "" | tee "$confmsgsave"
echo "Optional Features:" | tee -a "$confmsgsave"
echo "(1) Conversion between user or group ID and name .... $opt_ugidname" | tee -a "$confmsgsave"
echo "(2) Linux capabilities support ...................... $opt_cap" | tee -a "$confmsgsave"
echo "(3) Process monitor ................................. $opt_procmon" | tee -a "$confmsgsave"
echo "-------------------------------------------------------------" | tee -a "$confmsgsave"
echo "Compiling and Linking Options:" | tee -a "$confmsgsave"
echo "(1) Compiler optimization ........................... $opt_optimized" | tee -a "$confmsgsave"
echo "(2) Statically linked executable .................... $opt_static" | tee -a "$confmsgsave"
echo "-------------------------------------------------------------" | tee -a "$confmsgsave"
echo "Strings:" | tee -a "$confmsgsave"
echo "(1) Linux proc filesystem path ...................... $with_linuxproc" | tee -a "$confmsgsave"
echo "(2) Data sink path .................................. $with_null" | tee -a "$confmsgsave"
echo "The above messages has been written to $confmsgsave"
