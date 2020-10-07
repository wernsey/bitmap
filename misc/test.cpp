/*
 * Program to just make sure it plays nicely with C++ compilers.
 * (I've specifically tested it with g++)
 */
#include <iostream>

#define BMPH_IMPLEMENTATION
#include "../bmph.h"

int main() {
    std::cout << "Hello World" << std::endl;

    auto b = bm_create(100, 100);
    bm_set_color(b, bm_atoi("green"));
    bm_printf(b, 10, 10, "Hello C++");
    bm_save(b, "test.gif");
    bm_free(b);

    return 0;
}