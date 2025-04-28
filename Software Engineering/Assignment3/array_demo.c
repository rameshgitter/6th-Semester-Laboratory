#include <stdio.h>

int main(int argc, char *argv[]) {
    int arr[5] = {10, 20, 30, 40, 50};
    
    // Print function arguments
    printf("Number of arguments (argc): %d\n", argc);
    printf("First argument (argv[0]): %s\n", argv[0]);
    
    // Print array elements
    printf("Array elements:\n");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    return 0;
}

