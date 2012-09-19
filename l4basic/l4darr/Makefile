CC=cc
AR=ar
RM=rm
CFLAGS=-Wall -g -I.
OBJ=d1array.o d1arrstr.o d2array.o
LIBFILE=libl4darr.a

.PHONY: all clean

all: $(LIBFILE)
$(LIBFILE): $(OBJ)
	$(AR) rcs $(LIBFILE) $(OBJ)
clean:
	$(RM) -f $(LIBFILE) $(OBJ)
