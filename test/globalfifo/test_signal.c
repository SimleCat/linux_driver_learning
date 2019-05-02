#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>


static void sigio_handler(int signum)
{
	printf("recv a signal from globalfifo, signal num: %d\n", signum);

}



void main(void)
{
	int fd, oflags;

	fd = open("/dev/globalfifo", O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		printf("ERROR: open dev failed!\n");
		return;
	}

	signal(SIGIO, sigio_handler);
	fcntl(fd, F_SETOWN, getpid());
	oflags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, oflags | FASYNC);
	while(1) {
		sleep(100);
	}
}
