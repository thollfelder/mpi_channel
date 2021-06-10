#include "Chan.h"

// Impl Includes

#include "Impl1/Impl1.h"
#include "Impl2/Impl2.h"

Channel* create_something(int x) {
    if (x < 0) {
        create_something_impl1(x);
    }
    else {
        create_something_impl2(x);
    }
}

void delete_something(Channel* chan) {
    
}
