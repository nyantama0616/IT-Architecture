/*
例
gcc -o test_write test_write.c
./test_write /dev/device 77
*/

#include <stdio.h>
#include <stdlib.h>
// #include <ctype.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
// #include <string.h>

int main (int argc, char* argv[]) {
    int device_fd = open(argv[1], O_RDWR);
	if (device_fd < 0) {
		perror(argv[1]);
	}

    unsigned char led_state = (unsigned char)strtol(argv[2], NULL, 16);
	int ret;

    ret = write(device_fd, &led_state, 1);
    if (ret != 1) {
        perror("write");
        return 0;
    }
}
