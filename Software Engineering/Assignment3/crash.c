// crash.c
#include <stdio.h>
#include <stdlib.h>

void function3() {
    int *ptr = NULL;
    *ptr = 42;  // This will cause a segmentation fault.
}

void function2() {
    function3();
}

void function1() {
    function2();
}

int main() {
    function1();
    return 0;
}

