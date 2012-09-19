bin_PROGRAMS = sctjudge
sctjudge_CFLAGS = -Wall -pipe -pthread -I../l4basic/l4darr -I../l4basic/l4arg
sctjudge_LDFLAGS = -L../l4basic/l4darr -L../l4basic/l4arg
sctjudge_LDADD = -ll4arg -ll4darr
sctjudge_SOURCES = common.c common.h config.h core.h disptime.c checktle.c\
				   main.c mkchild.c version.h config2.h

if STATIC_EXEC
  sctjudge_LDFLAGS += -static
endif

install-exec-hook:
if USING_SETCAP
	setcap cap_sys_chroot,cap_setuid,cap_setgid,cap_kill+ep $(bindir)/sctjudge
else
	chown 0 $(bindir)/sctjudge
	chmod u+s $(bindir)/sctjudge
endif
