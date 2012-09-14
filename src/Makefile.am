bin_PROGRAMS = sctjudge
sctjudge_CFLAGS = -Wall -pipe -pthread
sctjudge_LDFLAGS = 
sctjudge_LDADD = 
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
