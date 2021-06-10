# define the C compiler to use
CC = mpicc

# define any compile-time flags
CFLAGS = -g -O -Wall -Wextra

# define any directories containing header files other than /usr/include
INCLUDES = #-I/Users/nguyenmanhduc/Documents/C\ library/cii/include

# define library paths in addition to /usr/lib
LFLAGS = #-L/Users/nguyenmanhduc/Documents/C\ library/cii/src

# define any libraries to link into executable:
LIBS = -lm

# define the C source files
# SRCS = um.c
SRCS = MPI_Channel_Throughput.c \
	src/MPI_Channel.c src/MPI_Channel_Struct.c \
	src/PT2PT/SPSC/PT2PT_SPSC_SYNC.c \
 	src/PT2PT/SPSC/PT2PT_SPSC_BUF.c \
	src/PT2PT/MPSC/PT2PT_MPSC_SYNC.c \
	src/PT2PT/MPSC/PT2PT_MPSC_BUF.c \
	src/PT2PT/MPMC/PT2PT_MPMC_SYNC.c \
	src/PT2PT/MPMC/PT2PT_MPMC_BUF.c \
	src/RMA/SPSC/RMA_SPSC_BUF.c \
	src/RMA/SPSC/RMA_SPSC_SYNC.c \
	src/RMA/MPSC/RMA_MPSC_BUF.c \
	src/RMA/MPSC/RMA_MPSC_SYNC.c \
	src/RMA/MPMC/RMA_MPMC_BUF.c \
	src/RMA/MPMC/RMA_MPMC_SYNC.c 

#IMPLS = 
#TESTS = Tests/ shared_2-sided_vs_1-sided.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = Test

.PHONY: depend clean

all:    $(MAIN)
	@echo  Test has been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^