#ifndef HOSTFS_INTERNAL_H
#define HOSTFS_INTERNAL_H

#include <stdint.h>

enum OBJECT_TYPE {
	OBJECT_TYPE_NOT_FOUND = 0,
	OBJECT_TYPE_FILE      = 1,
	OBJECT_TYPE_DIRECTORY = 2,
};

typedef struct {
	uint32_t	type;
	uint32_t	load;
	uint32_t	exec;
	uint32_t	length;
	uint32_t	attribs;
} risc_os_object_info;

extern void hostfs_read_object_info_platform(const char *host_pathname, risc_os_object_info *object_info);

extern void hostfs_object_set_timestamp_platform(const char *host_path, uint32_t load, uint32_t exec);

#endif
