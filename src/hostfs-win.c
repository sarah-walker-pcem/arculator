#include <assert.h>
#include <stdio.h>

#undef UNICODE
#include <Windows.h>

#include "arc.h"
#include "hostfs.h"
#include "hostfs_internal.h"

#define RISC_OS_TIME_EARLIEST	 94354848000000000ull	///< Earliest time in RISC OS, in FILETIME units
#define RISC_OS_TIME_LATEST	204306010777500000ull	///< Latest time in RISC OS, in FILETIME units

/**
 * Convert ADFS time-stamped Load-Exec addresses to the equivalent Windows FILETIME.
 *
 * @param      load RISC OS load address (assumed to be time-stamped)
 * @param      high RISC OS exec address (assumed to be time-stamped)
 * @param[out] ft   Pointer to Windows FILETIME, filled in with equivalent time
 */
static void
adfs_time_to_filetime(uint32_t load, uint32_t exec, FILETIME *ft)
{
	uint32_t low = exec;
	uint32_t high = load & 0xff;
	ULARGE_INTEGER ull;
	ULONGLONG centiseconds;

	assert(ft != NULL);

	centiseconds = (ULONGLONG) low | (((ULONGLONG) high) << 32);

	// Convert from centiseconds to 100-nanosecond intervals, and add offset
	ull.QuadPart = (centiseconds * 100000) + RISC_OS_TIME_EARLIEST;

	ft->dwLowDateTime = ull.LowPart;
	ft->dwHighDateTime = ull.HighPart;
}

/**
 * Convert a Windows FILETIME to the equivalent RISC OS time value.
 *
 * The Windows FILETIME has greater range and precision, so these may be lost
 * in the conversion.
 *
 * The RISC OS time is a 40-bit value, and returned in a pair of 32-bit integers.
 *
 * @param      ft   Pointer to Windows FILETIME
 * @param[out] low  Pointer to uint32_t, set to low 32 bits of RISC OS time
 * @param[out] high Pointer to uint32_t, set to high 8 bits of RISC OS time
 */
static void
filetime_to_adfs_time(const FILETIME *ft, uint32_t *low, uint32_t *high)
{
	ULARGE_INTEGER ull;
	ULONGLONG centiseconds;

	assert(ft != NULL);
	assert(low != NULL);
	assert(high != NULL);

	ull.LowPart = ft->dwLowDateTime;
	ull.HighPart = ft->dwHighDateTime;

	if (ull.QuadPart < RISC_OS_TIME_EARLIEST) {
		// Too early
		*low = 0;
		*high = 0;
		return;
	}
	if (ull.QuadPart > RISC_OS_TIME_LATEST) {
		// Too late
		*low = 0xffffffff;
		*high = 0xff;
	}

	// Subtract offset, and convert from 100-nanosecond intervals to centiseconds
	centiseconds = (ull.QuadPart - RISC_OS_TIME_EARLIEST) / 100000;

	assert(centiseconds <= 0xffffffffffull);

	*low = (uint32_t) centiseconds;
	*high = (uint32_t) (centiseconds >> 32);
}

/**
 * @param host_pathname Full Host path to object
 * @param object_info   Return object info (filled-in)
 */
static void
hostfs_read_object_info_fallback(const char *host_pathname,
                                 risc_os_object_info *object_info)
{
	HANDLE handle;
	WIN32_FIND_DATA info;
	uint32_t low, high;

	assert(host_pathname != NULL);
	assert(object_info != NULL);

	handle = FindFirstFile(host_pathname, &info);
	if (handle == INVALID_HANDLE_VALUE) {
		object_info->type = OBJECT_TYPE_NOT_FOUND;
		return;
	}

	/* Close handle */
	FindClose(handle);

	/* We were able to read about the object */
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		object_info->type = OBJECT_TYPE_DIRECTORY;
	} else {
		object_info->type = OBJECT_TYPE_FILE;
	}

	filetime_to_adfs_time(&info.ftLastWriteTime, &low, &high);

	/* If the file has filetype and timestamp, additional values will need to be filled in later */
	object_info->load = high;
	object_info->exec = low;

	object_info->length = info.nFileSizeLow;
}

/**
 * Read information about an object.
 *
 * @param host_pathname Full Host path to object
 * @param object_info   Return object info (filled-in)
 */
void
hostfs_read_object_info_platform(const char *host_pathname,
                                 risc_os_object_info *object_info)
{
	HANDLE handle;
	BY_HANDLE_FILE_INFORMATION info;
	uint32_t low, high;

	assert(host_pathname != NULL);
	assert(object_info != NULL);

	/* Get a handle to the object, but without needing Read/Write permissions */
	handle = CreateFile(host_pathname, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
	                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		/* Error opening the object */
		switch (GetLastError()) {
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
			/* Fallback to trying to read object information using FindFirstFile() */
			hostfs_read_object_info_fallback(host_pathname, object_info);
			return;

		case ERROR_FILE_NOT_FOUND:
		default:
			/* Other error */
			object_info->type = OBJECT_TYPE_NOT_FOUND;
			break;
		}

		return;
	}

	/* Read object info */
	if (GetFileInformationByHandle(handle, &info) == 0) {
		CloseHandle(handle);
		object_info->type = OBJECT_TYPE_NOT_FOUND;
		return;
	}

	/* Close object */
	CloseHandle(handle);

	/* We were able to read about the object */
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		object_info->type = OBJECT_TYPE_DIRECTORY;
	} else {
		object_info->type = OBJECT_TYPE_FILE;
	}

	filetime_to_adfs_time(&info.ftLastWriteTime, &low, &high);

	/* If the file has filetype and timestamp, additional values will need to be filled in later */
	object_info->load = high;
	object_info->exec = low;

	object_info->length = info.nFileSizeLow;
}

/**
 * Apply the timestamp to the supplied host object
 *
 * @param host_path Full path to object (file or dir) in host format
 * @param load      RISC OS load address (must contain time-stamp)
 * @param exec      RISC OS exec address (must contain time-stamp)
 */
void
hostfs_object_set_timestamp_platform(const char *host_path, uint32_t load, uint32_t exec)
{
	HANDLE handle;
	FILETIME ft;

	adfs_time_to_filetime(load, exec, &ft);

	/* Get a handle to the object, with permission to write attributes */
	handle = CreateFile(host_path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
	                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle != INVALID_HANDLE_VALUE) {
		SetFileTime(handle, NULL, &ft, &ft);
		CloseHandle(handle);
	}
}

/**
 * Return disk space information about a file system.
 *
 * @param path Pathname of object within file system
 * @param d    Pointer to disk_info structure that will be filled in
 * @return     On success 1 is returned, on error 0 is returned
 */
int
path_disk_info(const char *path, disk_info *d)
{
	ULARGE_INTEGER free, total;

	assert(path != NULL);
	assert(d != NULL);

	if (GetDiskFreeSpaceEx(path, &free, &total, NULL) == 0) {
		return 0;
	}

	d->size = (uint64_t) total.QuadPart;
	d->free = (uint64_t) free.QuadPart;

	return 1;
}
