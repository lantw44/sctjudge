CC=cc
AR=ar
RM=rm -f
INSTALL=install -m 644
LOCAL_CFLAGS=-g
CFLAGS=-Wall -I. $(LOCAL_CFLAGS)
OBJ=list.o
LIBFILE=libl4bds.a
DESTDIR=/
PREFIX=/usr/local
LIBDIR=$(DESTDIR)/$(PREFIX)/lib

.PHONY: all clean

all: $(LIBFILE)
$(LIBFILE): $(OBJ)
	$(AR) rcs $(LIBFILE) $(OBJ)
clean:
	$(RM) $(LIBFILE) $(OBJ)
install:
	mkdir -p $(LIBDIR)
	$(INSTALL) -c $(LIBFILE) $(LIBDIR)
uninstall:
	$(RM) $(LIBDIR)/$(LIBFILE)
