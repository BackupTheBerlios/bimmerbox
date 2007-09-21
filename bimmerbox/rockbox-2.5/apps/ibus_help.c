#define BUF_SIZE	4096

#include "file.h"
#include "string.h"

unsigned char buffer[BUF_SIZE];
static int w_ptr = 0;

static void fflush(int fd)
{
	write(fd, buffer, w_ptr);
	w_ptr = 0;
}

int fopen(char *filename, int flags)
{
	int fd = open(filename, flags);
	w_ptr = 0;
	return fd;
}

int fwrite(int fd, unsigned char *ptr, int len)
{
	if(w_ptr + len >= BUF_SIZE)
		fflush(fd);
	memcpy(buffer+w_ptr, ptr, len);
	w_ptr+=len;
	return len;
}

void fclose(int fd)
{
	// Let's flush the buffer
	fflush(fd);
	close(fd);

}
