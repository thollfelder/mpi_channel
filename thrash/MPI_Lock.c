#include "mpi.h"

static int MCS_LOCKRANK = MPI_KEYVAL_INVALID;
static int MAX_BUFFER_SIZE = 100;

enum
{
    nextRank = 0,
    size = 1,     //NEW
    count = 2,      // NEW
    blocked = 3,
    nextFailedSend = 4, // NEW
    nextFailedRecv = 5,  // NEW
    lockTail = 6

};

void MCSLockInit(MPI_Comm comm, MPI_Win *win)
{
    // Pointer to window memory and integer for rank of calling proc
    int *lmem, rank;

    // Size of window memory in bytes
    MPI_Aint winsize;

    // Get rank of calling proc
    MPI_Comm_rank(comm, &rank);

    // Create new keyval to store calling proc in a window attribute to access it later
    if (MCS_LOCKRANK == MPI_KEYVAL_INVALID) {
        MPI_Win_create_keyval(MPI_WIN_NULL_COPY_FN, MPI_WIN_NULL_DELETE_FN, &MCS_LOCKRANK, (void *)0);
    }

    // Allocate new window object with memory for two (rank != 0) or three integers (rank == 0) 
    winsize = 6 * sizeof(int);
    if (rank == 0)
        winsize += sizeof(int);

    MPI_Win_allocate(winsize, sizeof(int), MPI_INFO_NULL, comm, &lmem, win);

    // Every proc stores exactly one rank of the process which seeks to aquire the lock
    lmem[nextRank] = -1;
    lmem[blocked] = 0;

    // NEW
    lmem[size] = -1;
    lmem[count] = 0;
    lmem[nextFailedSend] = -1;
    lmem[nextFailedRecv] = -1;

    // Process 0 additionaly stores the process that most recently requested the lock
    if (rank == 0)
    {
        lmem[size] = 0;         // Size of proc 0 should be 0
        lmem[lockTail] = -1;
    }

    // Add rank of calling proc as window attribute
    MPI_Win_set_attr(*win, MCS_LOCKRANK, (void *)(MPI_Aint)rank);

    // Synchronize collective call
    MPI_Barrier(comm);
}

void MCSLockAcquire(MPI_Win win, int count)
{
    int flag, myrank, predecessor, *lmem;
    void *attrval;

    // new
    int old_size;

    // Get rank of calling proc, store it in myrank
    MPI_Win_get_attr(win, MCS_LOCKRANK, &attrval, &flag);
    myrank = (int)(MPI_Aint)attrval;

    // Get adress of the list element 
    MPI_Win_get_attr(win, MPI_WIN_BASE, &lmem, &flag);

    // Set count of local memory
    lmem[count] = count;

    // Signal that calling process is blocked now
    lmem[blocked] = 1; /* In case we are blocked */

    // Lock window on all procs of communicator with MPI_LOCK_SHARED
    MPI_Win_lock_all(0, win);

    // Get the lockTail of the list and replace it with the calling proc
    MPI_Fetch_and_op(&myrank, &predecessor, MPI_INT,
                     0, lockTail, MPI_REPLACE, win);

    // Enforce blocking until fetch_and_op is done
    MPI_Win_flush(0, win);

    // If the lockTail is not -1 another process holds the mutex
    if (predecessor != -1)
    {
        /* We didn’t get the lock. Add us to the tail of the list at the process recently requested locking (predecessor)  */
        // Add us to lmem[nextRank] at the proc recently requested locking
        MPI_Accumulate(&myrank, 1, MPI_INT, predecessor,
                       nextRank, 1, MPI_INT, MPI_REPLACE, win);
        /* Now spin on our local value "blocked" until we are
        given the lock */
        do
        {
            MPI_Win_sync(win); /* Ensure memory updated */
        } while (lmem[blocked] == 1);
    }
    // If lockTail was -1 the calling proc has the lock now

    // NEW: Atomic or non atomic get of the buffer size
    MPI_Get(&old_size, 1, MPI_INT, (predecessor == -1) ? 0 : predecessor, size, 1, MPI_INT, win);

    // NEW: Check if size + count is bigger than max buffer size or smaller than 0 
    int new_size = old_size + count;

    // NEW: count is invalid, size is too small or too big
    if ((new_size < 0) || (new_size > MAX_BUFFER_SIZE)) {

        // NEW: Set lmem[size] to size of predecessor
        lmem[size] = old_size;

        // NEW: While cas() != -1, lmem[failed{Recv|Send} = oldValue]
        // NEW: Block until lmem[blocked] = 0
        // NEW: lmem[blocked] = 1
        // NEW: wake up lmem[nextRank]

        return 0;
    }

    // NEW: Set local size to new_size
    lmem[size] = new_size;

    // NEW: According to sign of count check if there is a proc in lmem[nextFailedSend] at proc 0

    // keep the lock and do send receive operation
    MPI_Win_unlock_all(win);
}

void MCSLockRelease(MPI_Win win)
{
    int nullrank = -1, zero = 0, myrank, curtail, flag, *lmem;
    void *attrval;

    // Get rank of calling proc, store it in myrank
    MPI_Win_get_attr(win, MCS_LOCKRANK, &attrval, &flag);
    myrank = (int)(MPI_Aint)attrval;

    // Get adress of the list element 
    MPI_Win_get_attr(win, MPI_WIN_BASE, &lmem, &flag);

    // Lock window on all procs of communicator with MPI_LOCK_SHARED
    MPI_Win_lock_all(0, win);


    if (lmem[nextRank] == -1)
    {
        /* See if we’re waiting for the next to notify us */

        // Check if lmem[lockTail] at proc 0 holds the rank of the calling proc
        // If this is the case than replace that value with -1 indicating that no one is waiting for the mutex
        // Else do nothing and wait until lmem[nextRank] has been updated
        MPI_Compare_and_swap(&nullrank, &myrank, &curtail, MPI_INT,
                             0, lockTail, win);
        if (curtail == myrank)
        {
            /* We are the only process in the list */
            MPI_Win_unlock_all(win);
            return;
        }
        /* Otherwise, someone else has added themselves to the list.*/
        do
        {
            MPI_Win_sync(win);
        } while (lmem[nextRank] == -1);
    }

    /* Now we can notify them. Use accumulate with replace instead
of put since we want an atomic update of the location */

    // Notify next proc by replacing lmem[blocked] to 0 at proc lmem[nextRank]
    MPI_Accumulate(&zero, 1, MPI_INT, lmem[nextRank], blocked,
                   1, MPI_INT, MPI_REPLACE, win);
    MPI_Win_unlock_all(win);
}