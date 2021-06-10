#include "mpi.h"

static int MCS_LOCKRANK = MPI_KEYVAL_INVALID;

enum
{
    nextRank = 0,
    blocked = 1,
    lockTail = 2,
    //new
    capacity = 3
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
    winsize = 2 * sizeof(int);
    if (rank == 0)
        winsize += sizeof(int);

    MPI_Win_allocate(winsize, sizeof(int), MPI_INFO_NULL, comm, &lmem, win);

    // Every proc stores exactly one rank of the process which seeks to aquire the lock
    lmem[nextRank] = -1;
    lmem[blocked] = 0;

    // Process 0 additionaly stores the process that most recently requested the lock
    if (rank == 0)
    {
        lmem[lockTail] = -1;
    }

    // Add rank of calling proc as window attribute
    MPI_Win_set_attr(*win, MCS_LOCKRANK, (void *)(MPI_Aint)rank);

    // Synchronize collective call
    MPI_Barrier(comm);
}

void MCSLockAcquire(MPI_Win win)
{
    int flag, myrank, predecessor, *lmem;
    void *attrval;

    // Get rank of calling proc, store it in myrank
    MPI_Win_get_attr(win, MCS_LOCKRANK, &attrval, &flag);
    myrank = (int)(MPI_Aint)attrval;

    // Get adress of the list element 
    MPI_Win_get_attr(win, MPI_WIN_BASE, &lmem, &flag);

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
    // else we have the lock
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



/*
 *  AquireLock(win, count)
 *      lmem[blocked] (0) = 1
 *      FAO: Replace lastRank of proc 0 with own rank
 *      If lastRank == -1
 *          // Got lock
 *          Atomicget of Capacity from proc 0
 *          Check if count + capacity is valid
 *          If new capacity is valid:
 *              Set lmem[cap] (= -1) to new capacity
 *              Do send operation
 *          Else if new capacity is NOT valid:
 *              Check if lmem[lastRank] is -1
 *              If current proc is the only one in the list:
 *                  ReleaseLock or wait until another proc is seeking the lock
 *                  // Or put current proc into lastRank{Sender|Receiver} list and block
 *                  // or put process to end of list, check if count of blocked process is enough now
 *              If there is another proc seeking the lock:
 *                  // DBG check if new proc changes cap to valid cap, exchange position in list
 *                  // Think about two lmem[lastRank{Sender|Receiver} for failing sender and receiver to use as exchange backup 
 *                  Give lock to lmem[lastRank]
 *      Else if lastRank != -1
 *          // Another proc got lock
 *          Set nextRank of proc lastRank to proc of myRank
 *          Wait until capacity gets updated
 *
 * 
 * lmem[failedRecv]:        Stores the rank of a failed receiver process
 * lmem[lockTail]:          Stores the latest process seeking the lock
 * lmem[blocked]:           Holds 1 if process is blocked otherwise 0, blocked process spins on it
 * lmem[size]:              Stores the current size of the buffer
 * 
 * Description:
 * Smart MPI RMA Lock works as a combination of MCS Lock and Waiting Queue for Receiver Procs
 * Every process has local memory (abbrv. lmem) visible to other processes belonging to the same window. Each process stores the variables
 * blocked (initialized with 1, process spins on blocked until its is 0), nextRank (initialized with -1, gets updated with therank of the 
 * next process to notify), size (initialized with -1, used as information for the current buffer size, count (initialized with 0, used 
 * as information for how much the process wants to send or receive). 
 * 
 * 
 * Aquirelock(ch, lock)
 * - Set lmem[locked] of calling process to 1 to signal a locked state 
 * - Atomic fetch lmem[lockTail] from process 0 and replace it with myRank to signal that a new process wants the lock
 * - The old value of lmem[lockTail] from process 0 is called predecessor and will be -1 if the calling process is the first one or
 *  rank if another process was faster
 * 
 * - If predecessor != -1:
 *      - Set lmem[nextRank] of predecessor to myRank to let it notify the calling process
 *      - Block until predecessor updates lmem[size] of the calling process
 *      ... 
 *      - Do local calculations // TODO: What?
 *      ... 
 *      - Block until woken up from predecessor
 *       
 * - Else calling proc is the first proc in the list
 *      - Get the lmem[size] from process 0 (current size of the buffer)
 * 
 * - Calculate temporary new buffer size consisting of current buffer size + count (count can be negative (receive) or positive (send))
 * 
 * - If temporary new buffer size is invalid (temp_buf_size < 0 || temp_buf_size > buf_capacity):
 * 
 *      - If calling process wants to send:
 *          - Notify process lmem[nextRank] of calling process, if lmem[nextRank] is -1 assure, that no other process wants to aquire lock
 *          - Return -1 // TODO: What needs to be done to return safely?
 * 
 *      - If calling process wants to receive:
 *          - Update lmem[count] to count of calling process
 *          - Put process with CAS in list of failedProcs // TODO: More detailed description
 *          - Notify process lmem[nextRank] of calling process, if lmem[nextRank] is -1 assure, that no other process wants to aquire lock
 *          - Receiving process blocks and spins on lmem[blocked] until other process changes lmem[blocked] to 0 
 *          ...
 *          - When woken up, CAS lmem[failedRecv] of process 0 with rank of calling process
 *          - If they are the same, replace value with -1
 *          - If they are not the same, wait until lmem[failedRecv] has been updated and store lmem[failedRecv] of calling proc at proc 0
 * 
 * 
 * 
 * - If temporary new buffer size is valid (temp_buf_size >= 0 || temp_buf_size <= buf_capacity) && calling process is sending:
 * 
 *      - Check if a failed receiving process can succeed with new buffer size:
 *          - Atomic get the rank of lmem[failedRecv] of proc 0
 *          - If lmem[failedRecv] == -1:
 *              - Continue, since no proc has failed until now
 *          - Else:
 *              - Atomic fetch lmem[nextRank] from calling process and replace with rank of failed receiver process
 *              - If lmem[nextRank] was -1:
 *                  - Continue
 *              - Else
 *                  - Replace lmem[nextRank] of failed receiver process with old value of lmem[nextRank] of calling process
 *          - //TODO: Put rank of failedReceiver to lmem[failedReceiver] of process 0
 * 
 * 
 * - Lock aquired, do operation
 * 
 * 
 * 
 * 
 * 
 * - Atomic CAS/Update lmem[size] of lmem[nextRank] if not -1
 * - 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * Aquirelock(ch, lock)
 * - Set local locked to 1 to signal a locked state (lmem[blocked] = 1)
 * - Atomic fetch and replace lockTail at proc 0 with myRank to signal, that a new proc wants the lock
 * 
 * - If predecessor != -1 the calling proc needs to wait because another proc was faster
 *      - Set lmem[nextRank] of predecessor to myRank to let it wake me up when releasing the lock
 *      - Wait until predecessor updates lmem[size] of calling proc, then calling proc can continue // TODO: When must calling proc block?
 * 
 * - Else calling proc is the first proc in the list
 *      - Get current size of buffer from proc 0 (getSizeBuffer(rank_0, ...)
 * 
 * - Calculate temporary new buffer size consisting of current buffer size + count (count can be negative (receive) or positive (send))
 * 
 * - If temporary new buffer size is invalid (temp_buf_size < 0 || temp_buf_size > buf_capacity):
 *      - Put process with CAS in list of failedProcs
 *      - Block until lmem[nextRank] can be woken up
 *      - Block until woken up (then operation should work)
 *      - When woken up, CAS failedProc at proc 0 with failedProc at myrank // TODO: Rearrange list of failedProcs
 * 
 * - If temporary new buffer size is valid (temp_buf_size >= 0 || temp_buf_size <= buf_capacity):
 * 
 *      - Check if a failed process can succeed with new buffer size:
 * 
 *      - If calling proc's count is positive (will add to the buffer => Send):
 *          - Atomic get the rank of lmem[failedRecv] of proc 0
 *          - If lmem[failedRecv] == -1 continue, since no proc has failed until now
 *          - Fetch and replace lmem[nextRank] at myrank with rank of failed process
 *          - Replace lmem[nextRank] of failed process with previous rank of lmem[nextRank] at myrank if it wasnt -1
 * 
 *      - Else if calling proc's count is negative (will remove from the buffer => Receive):
 *          - Atomic get the rank of lmem[failedSend] of proc 0
 *          - If lmem[failedSend] == -1 continue, since no proc has failled until now
 *          - Fetch and replace lmem[nextRank] at myrank with rank of failed process
 *          - Replace lmem[nextRank] of failed process with previous rank of lmem[nextRank] at myrank if it wasnt -1
 * 
 * - Set lmem[size] to new valid buffer size
 * 
 * 
 * - Atomic CAS/Update lmem[size] of lmem[nextRank] if not -1
 * - 
 * 
 * 
 *      - Set lmem[size] to new valid size
 * 
 *      - Check if calling proc is last proc in list
 *      - If yes update lmem
 * 
 *      - If it was Send:
 *          - Check if Proc lmem[failedRead] at proc 0 works after send of calling proc
 *          - Would work:
 *              - Set lmem[nextRank] to proc lmem[failedRead] and if it was not -1 nextRank of failedRead to previous
 *          - Would not work:
 *              - continue
 *      - If it was Recv:
 *          - Vice versa
 * - If buffer is not valid:
 *      - CAS(FailedRead or FailedSend, -1, myRank) until lmem[failedRead/failedSend] == -1
 *      - Block until woken up
 *      - When woken up, wake next proc up
 *      - Block until woken up  
 * 
 * 
 *  Release: Check if lastRank is myRank then update lmem[cap] of proc 0
 * 
 */

 void MPI_Channel_Lock_Aquire(MPI_Win win, int count)
{
    int flag, myrank, predecessor, *lmem, capacity;
    void *attrval;

    // Get rank of calling proc, store it in myrank
    MPI_Win_get_attr(win, MCS_LOCKRANK, &attrval, &flag);
    myrank = (int)(MPI_Aint)attrval;

    // Get adress of the list element 
    MPI_Win_get_attr(win, MPI_WIN_BASE, &lmem, &flag);

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
    
    /*
    Get atomic current capacity of buffer
    MPI_Get_accumulate(&capacity, 1, MPI_INT, predecessor == -1 ? 0 : predecessor, capacity, )

    Check if count of buffer + count is valid
    if (count_buffer + ch->count >= 0 || < ch->capacity)
    {
        lmem[capacity] = count_buffer + ch->count

        MPI_Win_unlock_all(win);

        return and do send/receive

    }
    else {
        // Add us to lmem[nextFailed] at the proc recently requested locking
        MPI_Accumulate(&myrank, 1, MPI_INT, predecessor,
                       nextFailed, 1, MPI_INT, MPI_REPLACE, win);  
        // Now spin on our local value "blocked" until we are given the lock 
        do
        {
            MPI_Win_sync(win); // Ensure memory updated 
        } while (lmem[blocked] == 1);
    }

    // else we have the lock
    MPI_Win_unlock_all(win);
    }
*/
}



// Lock_Release:
// If there was a send, check failedReceive != -1 and look at that proc if new capacity is enough
// Vice versa if there was a receive
// if not put proc to nextRank list


