#include <stdio.h>
#include <time.h>
#include "src/MPI_Channel.h"

#define RECEIVER 1
#define SENDER 0

int assert(int line, int left, int right)
{
    if (left == right)
    {
        return 1;
    }
    else
    {
        printf("Assertion failed at: %i, Result: %i, Expected: %i\n", line, left, right);
        return 0;
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int ch1_size = 1, ch2_size = 5;
    int *data = malloc(sizeof(int) * ch2_size);

    for (int i = 0; i < ch2_size; i++)
    {
        data[i] = i + 1;
    }

    MPI_Channel *thrash, *ch1, *ch2;

    // Should return null
    thrash = channel_alloc(sizeof(int), 0, PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED, MPI_COMM_WORLD, RECEIVER);

    // Should return null
    thrash = channel_alloc(sizeof(int), -1, PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED, MPI_COMM_WORLD, RECEIVER);

    // Needed for testing
    ch1 = channel_alloc(sizeof(int), ch1_size, PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED, MPI_COMM_WORLD, RECEIVER);
    ch2 = channel_alloc(sizeof(int), ch2_size, PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED, MPI_COMM_WORLD, RECEIVER);

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with erroneous parameter
    if (rank == RECEIVER)
    {
        assert(__LINE__, channel_cansend(thrash), 0);
        assert(__LINE__, channel_canrecv(thrash), 0);
        assert(__LINE__, channel_peek(thrash, data), 0);
        assert(__LINE__, channel_receive(thrash, data), 0);
        assert(__LINE__, channel_receive_multiple(thrash, data, 1), 0);
        assert(__LINE__, channel_send(thrash, data), 0);
        assert(__LINE__, channel_send_multiple(thrash, data, 1), 0);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == SENDER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_cansend(thrash), 0);
        assert(__LINE__, channel_canrecv(thrash), 0);
        assert(__LINE__, channel_peek(thrash, data), 0);
        assert(__LINE__, channel_receive(thrash, data), 0);
        assert(__LINE__, channel_receive_multiple(thrash, data, 1), 0);
        assert(__LINE__, channel_send(thrash, data), 0);
        assert(__LINE__, channel_send_multiple(thrash, data, 1), 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases before first communication with valid param
    if (rank == RECEIVER)
    {
        assert(__LINE__, channel_cansend(ch1), 0);
        assert(__LINE__, channel_canrecv(ch1), 0);
        assert(__LINE__, channel_peek(ch1, data), 0);
        assert(__LINE__, channel_receive(ch1, data), 0);
        assert(__LINE__, channel_receive_multiple(ch1, data, 1), 0);
        assert(__LINE__, channel_send(ch1, data), 0);
        assert(__LINE__, channel_send_multiple(ch1, data, 1), 0);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == SENDER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_cansend(ch1), 1);
        assert(__LINE__, channel_canrecv(ch1), 0);
        assert(__LINE__, channel_peek(ch1, data), 0);
        assert(__LINE__, channel_receive(ch1, data), 0);
        assert(__LINE__, channel_receive_multiple(ch1, data, 1), 0);
        assert(__LINE__, channel_send(ch1, data), 1);
        assert(__LINE__, channel_send_multiple(ch1, data, 1), 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases after first send; now only operations the process can execute are tested
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch1), 0);
        assert(__LINE__, channel_send(ch1, data), 0);
        assert(__LINE__, channel_send_multiple(ch1, data, 1), 0);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch1), 1);
        assert(__LINE__, channel_peek(ch1, data), 1);
        assert(__LINE__, channel_receive(ch1, data), 1);
        assert(__LINE__, channel_receive_multiple(ch1, data, 1), 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send multiple and receive multiple
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch1), 1);
        assert(__LINE__, channel_send_multiple(ch1, data, 1), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch1), 1);
        assert(__LINE__, channel_peek(ch1, data), 1);
        assert(__LINE__, channel_receive_multiple(ch1, data, 1), 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send multiple and receive
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch1), 1);
        assert(__LINE__, channel_send_multiple(ch1, data, 1), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch1), 1);
        assert(__LINE__, channel_peek(ch1, data), 1);
        assert(__LINE__, channel_receive(ch1, data), 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send and receive multiple
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch1), 1);
        assert(__LINE__, channel_send(ch1, data), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch1), 1);
        assert(__LINE__, channel_peek(ch1, data), 1);
        assert(__LINE__, channel_receive_multiple(ch1, data, 1), 1);
    }

    // Test cases with bigger buffer size
    // Test cases with 1 send and receive multiple
    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases before first communication with valid param
    if (rank == RECEIVER)
    {
        assert(__LINE__, channel_cansend(ch2), 0);
        assert(__LINE__, channel_canrecv(ch2), 0);
        assert(__LINE__, channel_peek(ch2, data), 0);
        assert(__LINE__, channel_receive(ch2, data), 0);
        assert(__LINE__, channel_receive_multiple(ch2, data, 1), 0);
        assert(__LINE__, channel_send(ch2, data), 0);
        assert(__LINE__, channel_send_multiple(ch2, data, 1), 0);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == SENDER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_cansend(ch2), ch2_size);
        assert(__LINE__, channel_canrecv(ch2), 0);
        assert(__LINE__, channel_peek(ch2, data), 0);
        assert(__LINE__, channel_receive(ch2, data), 0);
        assert(__LINE__, channel_receive_multiple(ch2, data, 1), 0);
        assert(__LINE__, channel_send(ch2, data), 1);
        assert(__LINE__, channel_send_multiple(ch2, data, ch2_size - 1), ch2_size - 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases after first send; now only operations the process can execute are tested
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch2), 0);
        assert(__LINE__, channel_send(ch2, data), 0);
        assert(__LINE__, channel_send_multiple(ch2, data, 1), 0);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 1);
        assert(__LINE__, channel_peek(ch2, data), 1);
        assert(__LINE__, channel_receive(ch2, data), 1);
        assert(__LINE__, channel_canrecv(ch2), 4);
        assert(__LINE__, channel_receive_multiple(ch2, data, ch2_size - 1), ch2_size - 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send multiple and receive multiple
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch2), ch2_size);
        assert(__LINE__, channel_send_multiple(ch2, data, 1), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 1);
        assert(__LINE__, channel_peek(ch2, data), 1);
        assert(__LINE__, channel_receive_multiple(ch2, data, 1), 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send multiple and receive
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch2), ch2_size);
        assert(__LINE__, channel_send_multiple(ch2, data, 1), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 1);
        assert(__LINE__, channel_peek(ch2, data), 1);
        assert(__LINE__, channel_receive(ch2, data), 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with 1 send and receive multiple
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch2), ch2_size);
        assert(__LINE__, channel_send(ch2, data), 1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 1);
        assert(__LINE__, channel_peek(ch2, data), 1);
        assert(__LINE__, channel_receive_multiple(ch2, data, 1), 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Test cases with mixed send (multiple) and receive (multiple)
    if (rank == SENDER)
    {
        assert(__LINE__, channel_cansend(ch2), ch2_size);
        assert(__LINE__, channel_send_multiple(ch2, data, 2), 2);
        assert(__LINE__, channel_send(ch2, data), 1);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_cansend(ch2), ch2_size);
    }

    if (rank == RECEIVER)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 2);
        assert(__LINE__, channel_peek(ch2, data), 1);
        assert(__LINE__, channel_canrecv(ch2), 3);    // Peek needs to accept message and buffer them, canrecv increments
        assert(__LINE__, channel_receive_multiple(ch2, data, 2), 2);
        assert(__LINE__, channel_receive(ch2, data), 1);
        MPI_Barrier(MPI_COMM_WORLD);
        assert(__LINE__, channel_canrecv(ch2), 0);
    }

    MPI_Finalize();
}