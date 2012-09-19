CC=cc
AR=ar
RM=rm
CFLAGS=-Wall -g -I.
OBJ=list.o
LIBFILE=libl4bds.a

.PHONY: all clean

all: $(LIBFILE)
$(LIBFILE): $(OBJ)
	$(AR) rcs $(LIBFILE) $(OBJ)
clean:
	$(RM) -f $(LIBFILE) $(OBJ)
