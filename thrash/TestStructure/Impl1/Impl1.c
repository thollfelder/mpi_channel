#include "Impl1.h"

Channel* create_something_impl1(int x) {
    printf("Number created from Impl1: %i\n", x);
}

void delete_something_impl1(Channel* chan) {
    printf("Number deleted from Impl1\n");
}
