#include "fat32.h"
#include "lib.h"

#define SECTOR_SIZE 512
#define ATTR_LONG_NAME 0x0f
#define ATTR_DIRECTORY 0x10
#define FAT32_EOC 0x0ffffff8

static unsigned char sector[SECTOR_SIZE];
static unsigned char cluster_buf[32768];

static int is_fat32_type(uint8_t type)
{
	return type == 0x0b || type == 0x0c;
}

static int read_sector(uint64_t lba, void *buf)
{
	return read_disk_lba(lba, 1, buf);
}

static uint32_t cluster_lba(struct Fat32 *fs, uint32_t cluster)
{
	return fs->data_lba + (cluster - 2) * fs->sectors_per_cluster;
}

static uint32_t cluster_size(struct Fat32 *fs)
{
	return (uint32_t)fs->bytes_per_sector * fs->sectors_per_cluster;
}

static int read_cluster(struct Fat32 *fs, uint32_t cluster, void *buf)
{
	if (fs->bytes_per_sector != SECTOR_SIZE ||
	    cluster_size(fs) > sizeof(cluster_buf))
		return -1;

	return read_disk_lba(fs->part_lba + cluster_lba(fs, cluster),
			     fs->sectors_per_cluster, buf);
}

static int next_cluster(struct Fat32 *fs, uint32_t cluster, uint32_t *next)
{
	uint32_t fat_offset = cluster * 4;
	uint32_t fat_sector = fat_offset / fs->bytes_per_sector;
	uint32_t ent_offset = fat_offset % fs->bytes_per_sector;

	if (read_sector(fs->part_lba + fs->fat_lba + fat_sector, sector) != 0)
		return -1;

	*next = rd32(sector + ent_offset) & 0x0fffffff;
	return 0;
}

static char upper(char ch)
{
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 'A';
	return ch;
}

static void make_short_name(const char *name, char out[11])
{
	int i = 0;
	int j = 0;

	for (int k = 0; k < 11; k++)
		out[k] = ' ';

	while (name[i] && name[i] != '.' && j < 8)
		out[j++] = upper(name[i++]);

	if (name[i] == '.')
		i++;

	j = 8;
	while (name[i] && name[i] != '/' && j < 11)
		out[j++] = upper(name[i++]);
}

static int find_in_dir(struct Fat32 *fs, uint32_t dir_cluster, const char *name,
		       uint32_t *first_cluster, uint32_t *size, uint8_t *attr)
{
	char target[11];
	uint32_t cluster = dir_cluster;

	make_short_name(name, target);

	while (cluster < FAT32_EOC) {
		if (read_cluster(fs, cluster, cluster_buf) != 0)
			return -1;

		for (uint32_t off = 0; off < cluster_size(fs); off += 32) {
			unsigned char *ent = cluster_buf + off;

			if (ent[0] == 0x00)
				return -1;
			if (ent[0] == 0xe5 || ent[11] == ATTR_LONG_NAME)
				continue;
			if ((ent[11] & 0x08) != 0)
				continue;
			if (strncmp((const char *)ent, target, 11) != 0)
				continue;

			*attr = ent[11];
			*first_cluster = ((uint32_t)rd16(ent + 20) << 16) |
					 rd16(ent + 26);
			*size = rd32(ent + 28);
			return 0;
		}

		if (next_cluster(fs, cluster, &cluster) != 0)
			return -1;
	}

	return -1;
}

int fat32_mount_boot_partition(struct Fat32 *fs)
{
	uint64_t part_lba = 0;

	memset(fs, 0, sizeof(*fs));

	if (read_sector(0, sector) != 0)
		return -1;

	for (int pass = 0; pass < 2 && part_lba == 0; pass++) {
		for (int i = 0; i < 4; i++) {
			unsigned char *ent = sector + 446 + i * 16;
			uint8_t bootable = ent[0];
			uint8_t type = ent[4];

			if (!is_fat32_type(type))
				continue;
			if (pass == 0 && bootable != 0x80)
				continue;

			part_lba = rd32(ent + 8);
			break;
		}
	}

	if (part_lba == 0)
		return -1;

	if (read_sector(part_lba, sector) != 0)
		return -1;

	fs->part_lba = part_lba;
	fs->bytes_per_sector = rd16(sector + 11);
	fs->sectors_per_cluster = sector[13];
	fs->fat_count = sector[16];
	fs->sectors_per_fat = rd32(sector + 36);
	fs->root_cluster = rd32(sector + 44);
	fs->fat_lba = rd16(sector + 14);
	fs->data_lba = fs->fat_lba + fs->fat_count * fs->sectors_per_fat;

	if (fs->bytes_per_sector != SECTOR_SIZE || fs->sectors_per_cluster == 0 ||
	    cluster_size(fs) > sizeof(cluster_buf) ||
	    strncmp((const char *)sector + 82, "FAT32", 5) != 0)
		return -1;

	return 0;
}

int fat32_open(struct Fat32 *fs, const char *path, struct Fat32File *file)
{
	char component[16];
	uint32_t dir = fs->root_cluster;
	uint32_t first = 0;
	uint32_t size = 0;
	uint8_t attr = 0;
	int pos = 0;

	if (*path == '/')
		path++;

	while (1) {
		pos = 0;
		while (*path && *path != '/' && pos < (int)sizeof(component) - 1)
			component[pos++] = *path++;
		component[pos] = '\0';

		if (pos == 0)
			return -1;

		if (find_in_dir(fs, dir, component, &first, &size, &attr) != 0)
			return -1;

		if (*path == '/') {
			if ((attr & ATTR_DIRECTORY) == 0)
				return -1;
			dir = first;
			path++;
			continue;
		}

		if (attr & ATTR_DIRECTORY)
			return -1;

		file->fs = fs;
		file->first_cluster = first;
		file->size = size;
		return 0;
	}
}

int fat32_read_file(struct Fat32File *file, void *buf, uint32_t buf_size,
		    uint32_t *bytes_read)
{
	return fat32_read_at(file, 0, buf, buf_size, bytes_read);
}

int fat32_read_at(struct Fat32File *file, uint32_t offset, void *buf,
		  uint32_t size, uint32_t *bytes_read)
{
	struct Fat32 *fs = file->fs;
	uint32_t cluster = file->first_cluster;
	uint32_t csize = cluster_size(fs);
	uint32_t skip_clusters;
	uint32_t skip_bytes;
	uint32_t remaining;
	uint32_t out_remaining = size;
	char *out = buf;

	if (offset >= file->size) {
		*bytes_read = 0;
		return 0;
	}

	remaining = file->size - offset;
	if (remaining > size)
		remaining = size;

	skip_clusters = offset / csize;
	skip_bytes = offset % csize;

	while (skip_clusters--) {
		if (next_cluster(fs, cluster, &cluster) != 0)
			return -1;
	}

	while (remaining && out_remaining && cluster < FAT32_EOC) {
		uint32_t bytes = csize - skip_bytes;

		if (read_cluster(fs, cluster, cluster_buf) != 0)
			return -1;

		if (bytes > remaining)
			bytes = remaining;
		if (bytes > out_remaining)
			bytes = out_remaining;

		memcpy(out, cluster_buf + skip_bytes, bytes);
		out += bytes;
		remaining -= bytes;
		out_remaining -= bytes;
		skip_bytes = 0;

		if (remaining && out_remaining) {
			if (next_cluster(fs, cluster, &cluster) != 0)
				return -1;
		}
	}

	*bytes_read = size - out_remaining;
	return 0;
}

static int starts_with(const char *s, const char *prefix)
{
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void copy_token(char *dest, int dest_size, const char *src)
{
	int i = 0;

	while (*src == ' ' || *src == '\t')
		src++;
	while (*src && *src != '\r' && *src != '\n' && i < dest_size - 1)
		dest[i++] = *src++;
	dest[i] = '\0';
}

static void parse_cfg(char *text, struct BootConfig *cfg)
{
	char *line = text;

	memset(cfg, 0, sizeof(*cfg));

	while (*line) {
		char *next = line;

		while (*next && *next != '\n')
			next++;
		if (*next)
			*next++ = '\0';

		while (*line == ' ' || *line == '\t')
			line++;

		if (starts_with(line, "kernel=")) {
			copy_token(cfg->kernel, sizeof(cfg->kernel), line + 7);
		} else if (starts_with(line, "cmdline=")) {
			copy_token(cfg->cmdline, sizeof(cfg->cmdline), line + 8);
		} else if (starts_with(line, "linux ")) {
			char *p = line + 6;
			int i = 0;

			while (*p == ' ' || *p == '\t')
				p++;
			while (*p && *p != ' ' && *p != '\t' &&
			       i < (int)sizeof(cfg->kernel) - 1)
				cfg->kernel[i++] = *p++;
			cfg->kernel[i] = '\0';
			copy_token(cfg->cmdline, sizeof(cfg->cmdline), p);
		}

		line = next;
	}
}

int fat32_load_boot_config(struct Fat32 *fs, struct BootConfig *cfg)
{
	struct Fat32File file;
	char text[FAT32_MAX_CFG + 1];
	uint32_t bytes = 0;

	if (fat32_open(fs, "/BOOT.CFG", &file) != 0)
		return -1;
	if (fat32_read_file(&file, text, FAT32_MAX_CFG, &bytes) != 0)
		return -1;

	text[bytes] = '\0';
	parse_cfg(text, cfg);

	return cfg->kernel[0] ? 0 : -1;
}
