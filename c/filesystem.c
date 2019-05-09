#include "filesystem.h"
#include "util.h"

#include <string.h>
#include <stdio.h>

/* file_t info items */
#define FILE_T_ZERO_SECTOR 0
#define FILE_T_CURRENT_SECTOR 1
#define FILE_T_CURRENT_POSITION 2
#define FILE_T_FILE_POSITION 3

uint32_t get_free_sector()
{
	uint8_t root_buffer[SECTOR_SIZE] = {0};
	hdd_read(0, root_buffer);
	uint32_t free_sector_addr = ((root_sector_t *)root_buffer)->first_free_sector;

	uint8_t free_sector_buffer[SECTOR_SIZE] = {0};
	hdd_read(free_sector_addr, free_sector_buffer);

	((root_sector_t *)root_buffer)->first_free_sector = ((free_sector_t *)free_sector_buffer)->next_free_sector;

	hdd_write(0, root_buffer);
	return free_sector_addr;
}

uint32_t get_free_zero_sector()
{
	uint32_t file_addr = 0;
	uint8_t file_buffer[SECTOR_SIZE] = {0};
	hdd_read(file_addr, file_buffer);

	if (((root_sector_t *)file_buffer)->first_file_zero_sector == HDD_SIZE)
	{
		uint32_t new_zero_sector_addr = get_free_sector();
		hdd_read(file_addr, file_buffer);
		((root_sector_t *)file_buffer)->first_file_zero_sector = new_zero_sector_addr;
		hdd_write(0, file_buffer);
		return new_zero_sector_addr;
	}

	file_addr = ((root_sector_t *)file_buffer)->first_file_zero_sector;
	while (1)
	{
		hdd_read(file_addr, file_buffer);
		if (((file_zero_sector_t *)file_buffer)->next_file == HDD_SIZE)
		{
			uint32_t new_zero_sector_addr = get_free_sector();
			hdd_read(file_addr, file_buffer);
			((file_zero_sector_t *)file_buffer)->next_file = new_zero_sector_addr;
			hdd_write(file_addr, file_buffer);
			return new_zero_sector_addr;
		}

		file_addr = ((file_zero_sector_t *)file_buffer)->next_file;
	}
}

void free_sector(uint32_t allocated_sector_addr)
{
	uint8_t root_buffer[SECTOR_SIZE] = {0};
	hdd_read(0, root_buffer);

	uint8_t allocated_sector_buffer[SECTOR_SIZE] = {0};

	((free_sector_t *)allocated_sector_buffer)->next_free_sector = ((root_sector_t *)root_buffer)->first_free_sector;
	((root_sector_t *)root_buffer)->first_free_sector = allocated_sector_addr;

	hdd_write(0, root_buffer);
	hdd_write(allocated_sector_addr, allocated_sector_buffer);
}

void free_zero_sector(uint32_t allocated_sector_addr)
{
	uint32_t prev_file_addr = 0;
	uint8_t prev_file_buffer[SECTOR_SIZE] = {0};
	hdd_read(prev_file_addr, prev_file_buffer);

	uint32_t current_file_addr = ((root_sector_t *)prev_file_buffer)->first_file_zero_sector;
	uint8_t current_file_buffer[SECTOR_SIZE] = {0};
	hdd_read(current_file_addr, current_file_buffer);

	if (current_file_addr == allocated_sector_addr)
	{
		((root_sector_t *)prev_file_buffer)->first_file_zero_sector = ((file_zero_sector_t *)current_file_buffer)->next_file;
		hdd_write(prev_file_addr, prev_file_buffer);
		free_sector(current_file_addr);
		return;
	}

	while (1)
	{
		if (current_file_addr == HDD_SIZE)
			return;

		for (int i = 0; i < SECTOR_SIZE; i++)
			prev_file_buffer[i] = current_file_buffer[i];
		hdd_read(current_file_addr, current_file_buffer);

		if (current_file_addr == allocated_sector_addr)
		{
			((file_zero_sector_t *)prev_file_buffer)->next_file = ((file_zero_sector_t *)current_file_buffer)->next_file;
			hdd_write(prev_file_addr, prev_file_buffer);
			free_sector(current_file_addr);
			return;
		}
		prev_file_addr = current_file_addr;
		current_file_addr = ((file_zero_sector_t *)current_file_buffer)->next_file;
	}
}

uint32_t get_file_addr(const char *path)
{
	uint8_t buffer[SECTOR_SIZE] = {0};
	hdd_read(0, buffer);

	uint32_t file_sector_addr = ((root_sector_t *)buffer)->first_file_zero_sector;
	while (1)
	{
		if (file_sector_addr == HDD_SIZE)
			return FAIL;

		hdd_read(file_sector_addr, buffer);
		if (!strcmp(buffer, path))
			return file_sector_addr;

		file_sector_addr = ((file_zero_sector_t *)buffer)->next_file;
		if (file_sector_addr == 0)
			break;
	}
	return FAIL;
}

void fs_format()
{
	uint8_t buffer[SECTOR_SIZE] = {0};
	((root_sector_t *)buffer)->first_free_sector = 1;
	((root_sector_t *)buffer)->first_file_zero_sector = HDD_SIZE;
	hdd_write(0, buffer);
	for (int i = 1; i < HDD_SIZE; i++)
	{
		((free_sector_t *)buffer)->next_free_sector = i + 1;
		hdd_write(i, buffer);
	}
}

file_t *fs_creat(const char *path)
{
	if (strrchr(path, PATHSEP) != path || strlen(path) > FILENAME_MAX)
		return (file_t *)FAIL;

	uint32_t file_addr = get_file_addr(path);
	if (file_addr != FAIL)
		fs_unlink(path);

	uint32_t new_file_sector = get_free_zero_sector();
	uint8_t zero_sector_buffer[SECTOR_SIZE] = {0};
	hdd_read(new_file_sector, zero_sector_buffer);

	uint32_t data_sector_addr = get_free_sector();
	uint8_t data_sector_buffer[SECTOR_SIZE] = {0};
	hdd_read(data_sector_addr, data_sector_buffer);

	((file_zero_sector_t *)zero_sector_buffer)->size = 0;
	((file_zero_sector_t *)zero_sector_buffer)->next_file = HDD_SIZE;
	((file_zero_sector_t *)zero_sector_buffer)->file_first_data_sector = data_sector_addr;
	strcpy(zero_sector_buffer, path);

	hdd_write(new_file_sector, zero_sector_buffer);

	((file_data_sector_t *)data_sector_buffer)->next_file_sector = HDD_SIZE;
	((file_data_sector_t *)data_sector_buffer)->data[0] = '\0';
	hdd_write(data_sector_addr, data_sector_buffer);
	return fs_open(path);
}

file_t *fs_open(const char *path)
{
	uint32_t file_sector_addr = get_file_addr(path);
	if (file_sector_addr == FAIL)
		return (file_t *)FAIL;

	uint8_t file_buffer[SECTOR_SIZE] = {0};
	hdd_read(file_sector_addr, file_buffer);

	file_t *fd = fd_alloc();
	fd->info[FILE_T_ZERO_SECTOR] = file_sector_addr;
	fd->info[FILE_T_CURRENT_SECTOR] = ((file_zero_sector_t *)file_buffer)->file_first_data_sector;
	fd->info[FILE_T_CURRENT_POSITION] = 0;
	fd->info[FILE_T_FILE_POSITION] = 0;
	return fd;
}

int fs_close(file_t *fd)
{
	if (fd == NULL)
		return FAIL;
	fd_free(fd);
	return OK;
}

int fs_unlink(const char *path)
{
	uint32_t remove_file_sector_addr = get_file_addr(path);
	if (remove_file_sector_addr == FAIL)
		return FAIL;

	uint8_t remove_file_buffer[SECTOR_SIZE] = {0};
	hdd_read(remove_file_sector_addr, remove_file_buffer);

	uint32_t next_file_sector_addr = ((file_zero_sector_t *)remove_file_buffer)->file_first_data_sector;
	free_zero_sector(remove_file_sector_addr);

	while (1)
	{
		hdd_read(next_file_sector_addr, remove_file_buffer);
		uint32_t prev_file_sector_addr = next_file_sector_addr;
		next_file_sector_addr = ((file_data_sector_t *)remove_file_buffer)->next_file_sector;

		free_sector(prev_file_sector_addr);
		if (next_file_sector_addr == HDD_SIZE)
			return OK;
	}
};

int fs_rename(const char *oldpath, const char *newpath)
{
	uint32_t file_sector_addr = get_file_addr(oldpath);
	if (file_sector_addr == FAIL)
		return FAIL;

	uint8_t file_buffer[SECTOR_SIZE] = {0};
	hdd_read(file_sector_addr, file_buffer);

	strcpy(file_buffer, newpath);
	hdd_write(file_sector_addr, file_buffer);

	return OK;
};

int fs_read(file_t *fd, uint8_t *bytes, unsigned int size)
{
	uint8_t buffer[SECTOR_SIZE] = {0};
	hdd_read(fd->info[FILE_T_CURRENT_SECTOR], buffer);

	uint32_t current_position = fd->info[FILE_T_CURRENT_POSITION];
	uint32_t file_position = fd->info[FILE_T_FILE_POSITION];
	uint32_t counter = 0;
	if (buffer[current_position] == '\0')
		return 0;

	for (int i = 0; i < size; i++)
	{
		counter++;
		file_position++;
		bytes[i] = buffer[current_position];
		current_position++;

		if (current_position == (SECTOR_SIZE - INTEGER_SIZE - 1))
		{
			uint32_t next_file_sector = ((file_data_sector_t *)buffer)->next_file_sector;
			if (next_file_sector == HDD_SIZE)
				break;
			else
				hdd_read(next_file_sector, buffer);
			fd->info[FILE_T_CURRENT_SECTOR] = next_file_sector;
			current_position = 0;
		}
		else if (buffer[current_position] == '\0')
			break;
	}

	if (counter < strlen(bytes))
		bytes[counter] = '\0';

	fd->info[FILE_T_CURRENT_POSITION] = current_position;
	fd->info[FILE_T_FILE_POSITION] = file_position;
	return counter;
}

int fs_write(file_t *fd, const uint8_t *bytes, unsigned int size)
{
	uint8_t file_sector_buffer[SECTOR_SIZE] = {0};
	hdd_read(fd->info[FILE_T_CURRENT_SECTOR], file_sector_buffer);

	uint32_t current_position = fd->info[FILE_T_CURRENT_POSITION];
	uint32_t file_position = fd->info[FILE_T_FILE_POSITION];
	uint32_t counter = 0;

	for (int i = 0; i < size; i++)
	{
		file_sector_buffer[current_position] = bytes[i];
		counter++;
		file_position++;
		current_position++;

		if (current_position == (SECTOR_SIZE - INTEGER_SIZE - 1))
		{
			uint32_t new_sector_addr = get_free_sector();
			if (new_sector_addr == HDD_SIZE)
				return FAIL;
			((file_data_sector_t *)file_sector_buffer)->next_file_sector = new_sector_addr;
			hdd_write(fd->info[FILE_T_CURRENT_SECTOR], file_sector_buffer);
			fd->info[FILE_T_CURRENT_SECTOR] = new_sector_addr;
			hdd_read(new_sector_addr, file_sector_buffer);
			((file_data_sector_t *)file_sector_buffer)->next_file_sector = HDD_SIZE;
			current_position = 0;
		}
	}

	if (current_position <= (SECTOR_SIZE - INTEGER_SIZE - 1))
		file_sector_buffer[current_position] = '\0';

	hdd_write(fd->info[FILE_T_CURRENT_SECTOR], file_sector_buffer);
	fd->info[FILE_T_CURRENT_POSITION] = current_position;
	fd->info[FILE_T_FILE_POSITION] += counter;

	uint8_t zero_sector_buffer[SECTOR_SIZE] = {0};
	hdd_read(fd->info[FILE_T_ZERO_SECTOR], zero_sector_buffer);
	((file_zero_sector_t *)zero_sector_buffer)->size += counter;
	hdd_write(fd->info[FILE_T_ZERO_SECTOR], zero_sector_buffer);
	return counter;
}

int fs_seek(file_t *fd, unsigned int pos)
{
	uint8_t buffer[SECTOR_SIZE] = {0};
	hdd_read(fd->info[FILE_T_ZERO_SECTOR], buffer);

	if (((file_zero_sector_t *)buffer)->size <= pos)
		return FAIL;

	uint32_t current_position = 0;
	uint32_t current_addr = ((file_zero_sector_t *)buffer)->file_first_data_sector;
	hdd_read(current_addr, buffer);

	while (strlen(((file_data_sector_t *)buffer)->data) + current_position <= pos)
	{
		current_position += strlen(((file_data_sector_t *)buffer)->data);
		current_addr = ((file_data_sector_t *)buffer)->next_file_sector;
		if (current_addr == HDD_SIZE)
			return FAIL;
		hdd_read(current_addr, buffer);
	}

	fd->info[FILE_T_CURRENT_SECTOR] = current_addr;
	fd->info[FILE_T_CURRENT_POSITION] = pos % (SECTOR_SIZE - INTEGER_SIZE - 1);
	fd->info[FILE_T_FILE_POSITION] = pos;
	return OK;
}

unsigned int fs_tell(file_t *fd)
{
	return fd->info[FILE_T_FILE_POSITION];
}

int fs_stat(const char *path, fs_stat_t *fs_stat)
{
	uint32_t file_sector_addr = get_file_addr(path);
	if (file_sector_addr == FAIL)
		return FAIL;

	uint8_t file_buffer[SECTOR_SIZE] = {0};
	hdd_read(file_sector_addr, file_buffer);

	fs_stat->st_size = ((file_zero_sector_t *)file_buffer)->size;
	fs_stat->st_nlink = 1;
	fs_stat->st_type = ST_TYPE_FILE;

	return OK;
};

/* Level 3 */
/**
 * Vytvori adresar 'path'.
 *
 * Ak cesta, v ktorej adresar ma byt, neexistuje, vrati FAIL (vytvara najviac
 * jeden adresar), pri korektnom vytvoreni OK.
 */
int fs_mkdir(const char *path) { return FAIL; };

/**
 * Odstrani adresar 'path'.
 *
 * Odstrani adresar, na ktory ukazuje 'path'; ak neexistuje alebo nie je
 * adresar, vrati FAIL; po uspesnom dokonceni vrati OK.
 */
int fs_rmdir(const char *path) { return FAIL; };

/**
 * Otvori adresar 'path' (na citanie poloziek)
 *
 * Vrati handle na otvoreny adresar s poziciou nastavenou na 0; alebo FAIL v
 * pripade zlyhania.
 */
file_t *fs_opendir(const char *path) { return (file_t *)FAIL; };

/**
 * Nacita nazov dalsej polozky z adresara.
 *
 * Do dodaneho buffera ulozi nazov polozky v adresari a posunie aktualnu
 * poziciu na dalsiu polozku. V pripade problemu, alebo nemoznosti precitat
 * polozku (dalsia neexistuje) vracia FAIL.
 */
int fs_readdir(file_t *dir, char *item) { return FAIL; };

/**
 * Zatvori otvoreny adresar.
 */
int fs_closedir(file_t *dir) { return FAIL; };

/* Level 4 */
/**
 * Vytvori hardlink zo suboru 'path' na 'linkpath'.
 */
int fs_link(const char *path, const char *linkpath) { return FAIL; };

/**
 * Vytvori symlink z 'path' na 'linkpath'.
 */
int fs_symlink(const char *path, const char *linkpath) { return FAIL; };
