#include "util.h"
#include "filesystem.h"
#define INTEGER_SIZE 4

int main(void)
{
	file_t *fd;
	util_init("disk.bin", 65536);
	fs_format();

	fd = fs_creat("/test.txt");

	fs_write(fd, "abcdefghijkl", 12);
	fs_seek(fd, 13);
	printf("%d %d %d %d\n", fd->info[0], fd->info[1], fd->info[2], fd->info[3]);
	fs_seek(fd, 11);
	printf("%d %d %d %d\n", fd->info[0], fd->info[1], fd->info[2], fd->info[3]);
	fs_close(fd);

	util_end();
}
