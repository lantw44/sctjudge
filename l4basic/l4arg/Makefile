CC=cc
AR=ar
RM=rm
CFLAGS=-Wall -g -I. -I../l4darr
OBJ=toargv.o qarg.o
LIBFILE=libl4arg.a

.PHONY: all clean

all: $(LIBFILE)
$(LIBFILE): $(OBJ)
	$(AR) rcs $(LIBFILE) $(OBJ)
clean:
	$(RM) -f $(LIBFILE) $(OBJ)
