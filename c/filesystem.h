#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#define INTEGER_SIZE 4
#define MAX_FILENAME 12 /* Maximalna dlzka nazvu suboru v znakoch */
#define MAX_PATH 64		/* Maximalna dlzka cesty v znakoch */
#define SECTOR_SIZE 128 /* Velkost sektora na disku v bajtoch */

/* Oddelovac poloziek v adresarovej strukture */
#define PATHSEP '/'

#define OK 0
#define FAIL -1

/* Typy pre fs_stat_t.st_type */
#define ST_TYPE_FILE 0
#define ST_TYPE_DIR 1
#define ST_TYPE_SYMLINK 2

typedef struct
{
	uint32_t first_file_zero_sector;
	uint32_t first_free_sector;
} root_sector_t;

typedef struct
{
	char name[MAX_FILENAME];
	uint32_t size;
	uint32_t file_first_data_sector;
	uint32_t next_file;
} file_zero_sector_t;

typedef struct
{
	char data[SECTOR_SIZE - INTEGER_SIZE];
	uint32_t next_file_sector;
} file_data_sector_t;

typedef struct
{
	uint32_t next_free_sector;
} free_sector_t;

typedef struct
{
	uint32_t st_size;
	uint32_t st_nlink;
	uint32_t st_type;
} fs_stat_t;

/* File handle pre otvorenu polozku. Na kazdy otvoreny subor mate 16 bajtov, v
 * ktorych si mozete pamatat lubovolne informacie
 */
typedef struct
{
	uint32_t info[4];
} file_t;

/* Level 1 */

void fs_format();

file_t *fs_creat(const char *path);
file_t *fs_open(const char *path);
int fs_close(file_t *fd);
int fs_unlink(const char *path);
int fs_rename(const char *oldpath, const char *newpath);

/* Level 2 */
int fs_read(file_t *fd, uint8_t *bytes, unsigned int size);
int fs_write(file_t *fd, const uint8_t *bytes, unsigned int size);
int fs_seek(file_t *fd, unsigned int pos);
unsigned int fs_tell(file_t *fd);
int fs_stat(const char *path, fs_stat_t *fs_stat);

/* Level 3 */
int fs_mkdir(const char *path);
int fs_rmdir(const char *path);
file_t *fs_opendir(const char *path);
int fs_readdir(file_t *dir, char *item);
int fs_closedir(file_t *dir);

/* Level 4 */
int fs_link(const char *path, const char *linkpath);
int fs_symlink(const char *path, const char *linkpath);

#endif