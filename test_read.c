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

    unsigned char led_state = 0U;
	int ret;

    ret = read(device_fd, &led_state, 1);
    if (ret != 1) {
        perror("read");
        return 0;
    }

    printf("led_state = %x\n", led_state);
}
