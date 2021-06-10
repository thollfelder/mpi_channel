#include "src/MPI_Channel.h"
#include <stdio.h>

#define LOOPS 10

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *numbers = malloc(sizeof(int) * LOOPS);
    
    // RECEIVER 1
    if (rank == 0)
    {
        MPI_Channel *ch = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 1);
        printf("Rank %d: Receivercount: %d, Sendercount: %d, Receiverrank: %d, Senderrank: %d\n", rank, ch->receiver_count,
        ch->sender_count, ch->receiver_ranks[0], ch->sender_ranks[0]);

        for (int i = 0; i < LOOPS; i++) {
            channel_receive(ch, numbers+i);
            printf("Rank %d received %d\n", rank, numbers[i]);
        }
        printf("RECEIVER %d finished\n", rank);

       channel_free(ch);
    }

    // RECEIVER 2
    if (rank == 1)
    {
        MPI_Channel *ch = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 1);
        printf("Rank %d: Receivercount: %d, Sendercount: %d, Receiverrank: %d, Senderrank: %d\n", rank, ch->receiver_count,
        ch->sender_count, ch->receiver_ranks[0], ch->sender_ranks[0]);


        for (int i = 0; i < LOOPS; i++) {
            channel_receive(ch, numbers+i);
            printf("Rank %d received %d\n", rank, numbers[i]);
        }
        printf("RECEIVER %d finished\n", rank);

       channel_free(ch);
    }

    // SENDER 1
    if (rank == 2)
    {
        MPI_Channel *ch = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 0);
        printf("Rank %d: Receivercount: %d, Sendercount: %d, Receiverrank: %d, Senderrank: %d\n", rank, ch->receiver_count,
        ch->sender_count, ch->receiver_ranks[0], ch->sender_ranks[0]);

        for (int i = 0; i < LOOPS; i++) {
            numbers[i] = i+1;
            channel_send(ch, numbers+i);
            printf("Rank %d sent %d\n", rank, numbers[i]);
        }

        printf("SENDER %d finished\n", rank);

        channel_free(ch);
    }

    // SENDER 2
    if (rank == 3)
    {
        MPI_Channel *ch = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 0);
        printf("Rank %d: Receivercount: %d, Sendercount: %d, Receiverrank: %d, Senderrank: %d\n", rank, ch->receiver_count,
        ch->sender_count, ch->receiver_ranks[0], ch->sender_ranks[0]);

        for (int i = 0; i < LOOPS; i++) {
            numbers[i] = i+1;
            channel_send(ch, numbers+i);
            printf("Rank %d sent %d\n", rank, numbers[i]);
        }

        printf("SENDER %d finished\n", rank);
        
        channel_free(ch);
    }


    MPI_Finalize();

    return 0;
}
