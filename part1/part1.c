#include <unistd.h>

int main(void) {
    getpid();
    getuid();
    getgid();
    geteuid();
    getegid();
    return 0;
}
