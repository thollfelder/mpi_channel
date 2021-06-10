#include <stdio.h>
#include "src/MPI_Channel.h"

typedef struct sum
{
    int num_a;
    int num_b;
    char op;
} sum;

int main(int argc, char **argv)
{

    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Channel *ch1 = channel_alloc(sizeof(sum), 0, PT2PT_SPSC_SYNCHRONIZED, MPI_COMM_WORLD, 1);
    MPI_Channel *ch2 = channel_alloc(sizeof(sum), 0, PT2PT_SPSC_SYNCHRONIZED, MPI_COMM_WORLD, 1);

    sum send_sum_1 = {1, 1, '+'}, send_sum_2 = {40, 2, '+'};
    sum recv_sum_1, recv_sum_2;
    sum send_sums[2] = {{1, 1, '+'}, {2, 2, '+'}};
    sum recv_sums[2];

    if (rank == 0)
    {
        printf("Sending the sum %i %c %i over channel 1\n", send_sum_1.num_a, send_sum_1.op, send_sum_1.num_b);
        channel_send(ch1, &send_sum_1);
        printf("Sending the sum %i %c %i over channel 2\n", send_sum_2.num_a, send_sum_2.op, send_sum_2.num_b);
        channel_send(ch2, &send_sum_2);
        printf("Sending multiple sums: %i %c %i and %i %c %i over channel 1\n", send_sums[0].num_a, send_sums[0].op, send_sums[0].num_b,
               send_sums[1].num_a, send_sums[1].op, send_sums[1].num_b);
        channel_send_multiple(ch1, &send_sums, 2);
    }

    if (rank == 1)
    {
        // Wait for receivable messages
        while (!channel_peek(ch1))
        {
            // do stuff
        }
        channel_receive(ch1, &recv_sum_1);
        printf("Received the sum %i %c %i over channel 1\n", recv_sum_1.num_a, recv_sum_1.op, recv_sum_1.num_b);

        // Wait for receivable messages
        while (!channel_peek(ch2))
        {
            // do stuff
        }
        channel_receive(ch2, &recv_sum_2);
        printf("Received the sum %i %c %i over channel 2\n", recv_sum_2.num_a, recv_sum_1.op, recv_sum_2.num_b);

        // Wait for receivable messages
        while (!channel_canrecv(ch1))
        {
            // do stuff
        }
        channel_receive_multiple(ch1, &recv_sums, 2);
        printf("Received multiple sums: %i %c %i and %i %c %i over channel 1\n", recv_sums[0].num_a, recv_sums[0].op, recv_sums[0].num_b,
               recv_sums[1].num_a, recv_sums[1].op, recv_sums[1].num_b);
    }

    MPI_Finalize();
}