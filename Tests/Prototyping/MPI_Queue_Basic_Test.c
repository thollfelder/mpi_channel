#include <stdio.h>
#include "src/MPI_Channel.h"
#include "src/Queue/MPI_Queue.h"

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    MPI_Queue* qu = queue_alloc(sizeof(int), 2);
    int x = 5;
    queue_write(qu, &x);
    x = 6;
    queue_write(qu, &x);

    queue_read(qu, &x);
    printf("Read: %i \n", x);
    queue_read(qu, &x);
    printf("Read: %i \n", x);


    printf("Peek Queue: %i\n", queue_peek(qu));

    x = 10;
    queue_write(qu, &x);

    printf("Peek Queue: %i\n", queue_peek(qu));
    printf("Check Queue: %i\n", queue_check(qu));

    queue_read(qu, &x);
    printf("Read: %i \n", x);

    printf("Peek Queue: %i\n", queue_peek(qu));
    printf("Check Queue: %i\n", queue_check(qu));

    int arr[2] = {9, 99};

    queue_write_multiple(qu, &arr, 3);

    arr[0] = arr[1] = -1;

    printf("Read: %i\n", queue_read_multiple(qu, &arr, 2));
    printf("Read: %i %i\n", arr[0], arr[1]);


    queue_free(qu);

    MPI_Finalize();
}