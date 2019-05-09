#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"

static FILE *disk = NULL;
static unsigned int disk_size = 0;
int util_fd_total, util_fd_used;
static unsigned long read_counter = 0, write_counter = 0;

void util_reset_counters()
{
	read_counter = 0;
	write_counter = 0;
}

unsigned long util_get_reads() { return read_counter; }
unsigned long util_get_writes() { return write_counter; }

int util_init(const char *disk_path, unsigned int my_size)
{
	uint8_t buf[SECTOR_SIZE] = {0};
	assert(disk == NULL);
	assert((my_size % SECTOR_SIZE) == 0);

	util_fd_total = 16;
	util_fd_used = 0;

	disk_size = my_size;

	disk = fopen(disk_path, "rb+");
	if (disk == NULL)
	{
		int i;
		/* Initialize new blank hdd */
		disk = fopen(disk_path, "w+");
		if (!disk)
		{
			perror("util_init");
			return -1;
		};
		for (i = 0; i < hdd_size() / SECTOR_SIZE; i++)
			assert(fwrite(buf, SECTOR_SIZE, 1, disk) == 1);

		fs_format();
	}
	else
	{
		fseek(disk, 0, SEEK_END);
		int old_disk_size = ftell(disk);
		assert(hdd_size() == old_disk_size);
	}

	if (!disk)
	{
		perror("fopen");
		return -1;
	}

	return 0;
}

void util_end()
{
	fclose(disk);
	disk = NULL;
}

void hdd_read(unsigned int sector, void *buffer)
{
	int data_read;
	assert(disk != NULL);
	assert(sector < hdd_size() / SECTOR_SIZE);

	assert(fseek(disk, sector * SECTOR_SIZE, 0) == 0);
	data_read = fread(buffer, SECTOR_SIZE, 1, disk);

	assert(data_read == 1);
	read_counter++;
}

void hdd_write(unsigned int sector, void *buffer)
{
	int data_written;
	assert(disk != NULL);
	assert(sector < hdd_size() / SECTOR_SIZE);

	assert(fseek(disk, sector * SECTOR_SIZE, 0) == 0);
	data_written = fwrite(buffer, SECTOR_SIZE, 1, disk);

	assert(data_written == 1);
	fflush(disk);
	write_counter++;
}

unsigned int hdd_size() { return disk_size; };

file_t *fd_alloc()
{
	assert((util_fd_total - util_fd_used) > 0);

	file_t *ret = (file_t *)malloc(sizeof(file_t));
	assert(ret != NULL);

	util_fd_used++;
	return ret;
}

void fd_free(file_t *fd)
{
	assert(fd != NULL);
	assert(util_fd_used > 0);
	free(fd);
	util_fd_used--;
}

void print_system()
{
	uint8_t root_buffer[SECTOR_SIZE] = {0};
	hdd_read(0, root_buffer);
	printf("Root sector:\n");
	printf("  first_file_zero_sector: %d\n", ((root_sector_t *)root_buffer)->first_file_zero_sector);
	printf("  first_free_sector: %d\n", ((root_sector_t *)root_buffer)->first_free_sector);
	printf("Files:\n");
	uint32_t zero_sector_addr = ((root_sector_t *)root_buffer)->first_file_zero_sector;
	uint8_t file_buffer[SECTOR_SIZE] = {0};
	uint8_t file_data_buffer[SECTOR_SIZE] = {0};

	while (1)
	{
		printf("\n");
		if (zero_sector_addr == HDD_SIZE || zero_sector_addr == 0)
			break;
		hdd_read(zero_sector_addr, file_buffer);
		printf(" File %s\n", ((file_zero_sector_t *)file_buffer)->name);
		printf(" Size %d\n", ((file_zero_sector_t *)file_buffer)->size);
		printf(" First data sector %d\n", ((file_zero_sector_t *)file_buffer)->file_first_data_sector);
		printf(" Next file %d\n", ((file_zero_sector_t *)file_buffer)->next_file);
		uint32_t data_sector_addr = ((file_zero_sector_t *)file_buffer)->file_first_data_sector;

		while (1)
		{
			if (data_sector_addr == HDD_SIZE || data_sector_addr == 0)
				break;
			hdd_read(data_sector_addr, file_data_buffer);
			printf(" Data sector:\n");
			printf("   Data %s\n", ((file_data_sector_t *)file_data_buffer)->data);
			printf("   Next sector %d\n", ((file_data_sector_t *)file_data_buffer)->next_file_sector);
			data_sector_addr = ((file_data_sector_t *)file_data_buffer)->next_file_sector;
		}
		zero_sector_addr = ((file_zero_sector_t *)file_buffer)->next_file;
	}
}