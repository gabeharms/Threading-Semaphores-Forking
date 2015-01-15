# CMPSC 473, Project 1, starter kit

# Source, library, include, and object files
SRC = pr1.c
LIB =
INC = 
OBJ = pr1pipe
CFLAGS = -Wall -Wextra -std=c99 -pthread
# Different versions of the POSIX Standard for Unix
POSIX_2001 = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
POSIX_2008 = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700

# set the default action to do nothing

# Suffix rules
.SUFFIXES: .c .o

.c.o:
	gcc $(CFLAGS)  -o $@ $<

# Compile for Solaris, Linux, Mac OS X; all warnings turned onX

linux: $(SRC) $(LIB) $(INC)
	gcc  $(POSIX_2008) $(CFLAGS) -o $(OBJ) $(SRC) $(LIB)

clean:
	rm $(OBJ)

pr1pipe.o: pr1pipe.c
