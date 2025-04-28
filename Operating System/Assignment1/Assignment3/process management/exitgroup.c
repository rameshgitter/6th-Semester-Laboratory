#include <linux/unistd.h> // For SYS_exit_group
#include <sys/syscall.h>  // For syscall() prototype

int main() {
    syscall(SYS_exit_group, 0); // Exit with status 0
    return 0; // This line will not be executed
}

