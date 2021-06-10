#include <sys/time.h>
#include <stdio.h>

#include "src/MPI_Channel.h"

int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm shadow_comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &shadow_comm);

    if (rank == 0) {
        int x = 5, y = 6;
        MPI_Send(&x, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&y, 1, MPI_INT, 1, 0, shadow_comm);
    }
    if (rank == 1) {
        sleep(1);
        int x, y;
        MPI_Recv(&x, 1, MPI_INT, 0, 0, shadow_comm, MPI_STATUS_IGNORE);
        printf("Receiving from shadowcomm: %i\n", x);
        MPI_Recv(&y, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Receiving from comm world: %i\n", y);
    }
    MPI_Finalize();
}