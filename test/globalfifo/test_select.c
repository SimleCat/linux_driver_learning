#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>


#define BUF_LEN		20

void main(void)
{
	int fd, num;
	char buf[BUF_LEN];
	fd_set rfds, wfds;

	fd = open("/dev/globalfifo", O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("open dev failed!\n");
		return;
	}

	while (1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(fd, &rfds);
		FD_SET(fd, &wfds);
	
		select(fd + 1, &rfds, &wfds, NULL, NULL);

		if (FD_ISSET(fd, &rfds))
			printf("Poll monitor: can be read\n");
		/*
		if (FD_ISSET(fd, &wfds))
			printf("Poll monitor: can be waite\n");
		*/
	}
}
