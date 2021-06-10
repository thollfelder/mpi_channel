# define the C compiler to use
CC = gcc

# define any compile-time flags
CFLAGS = -g -O -Wall -Wextra

# define any directories containing header files other than /usr/include
INCLUDES = -I/Users/nguyenmanhduc/Documents/C\ library/cii/include

# define library paths in addition to /usr/lib
LFLAGS = -L/Users/nguyenmanhduc/Documents/C\ library/cii/src

# define any libraries to link into executable:
LIBS = -lm

# define the C source files
# SRCS = um.c
SRCS = Test.c Chan.c Impl1/Impl1.c Impl2/Impl2.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = um

.PHONY: depend clean

all:    $(MAIN)
	@echo  Simple compiler named um has been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^