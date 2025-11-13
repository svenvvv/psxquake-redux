#include "psx/io.h"
#include "psx/files.h"
#include "util/hashlib.h"
#include "sys.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <psxcd.h>

#define PSX_CD_SECTOR_SIZE 2048
#define PSX_CD_FILES_MAX 4
#define PSX_MEMCARD_FILES_MAX 4

#define psx_file_is_valid(_file_handle) \
    (psx_files <= _file_handle || _file_handle < &psx_files[ARRAY_SIZE(psx_files)])

typedef struct psx_cd_file_s
{
    /** Whether the file has been allocated */
	bool allocated;
    /** Hash of the filename */
	uint32_t filename_hash;
	/** Current read cursor, in CD-drive sectors */
	size_t cursor_sectors;
	/** Current read cursor offset, in bytes */
	size_t cursor_bytes;
    /** PSX SDK file structure */
	CdlFILE file;
    /** Whether the read buffer contains valid data */
    int read_buf_is_valid;
    uint8_t read_buf[PSX_CD_SECTOR_SIZE];
} psx_cd_file;

static psx_cd_file psx_files[PSX_CD_FILES_MAX] = { 0 };

void * psx_file_duplicate_handle(uint32_t filename_hash)
{
    // Quake uses fopen to get a second file handle for pak files
    for (int i = 0; i < ARRAY_SIZE(psx_files); ++i) {
        psx_cd_file const * f = &psx_files[i];

        if (!f->allocated || f->filename_hash != filename_hash) {
            continue;
        }

        // If we have the file already open then allocate a new handle and copy the contents over
        psx_cd_file * ret = psx_file_alloc();
        if (ret == NULL) {
            printf("psx_file_duplicate_handle: no available file handles\n");
            return NULL;
        }
        ret->filename_hash = f->filename_hash;
        memcpy(&ret->file, &f->file, sizeof(ret->file));
        return ret;
    }
    printf("psx_file_duplicate_handle: file not found\n");
    return NULL;
}

void * psx_file_alloc(void)
{
    psx_cd_file *ret = NULL;
    // EnterCriticalSection();
    for (int i = 0; i < ARRAY_SIZE(psx_files); ++i) {
        if (psx_files[i].allocated) {
            continue;
        }
        ret = &psx_files[i];
        // memset(ret, 0, sizeof(*ret));
        ret->allocated = true;
        ret->cursor_bytes = 0;
        ret->cursor_sectors = 0;
        ret->read_buf_is_valid = false;
        break;
    }
    // ExitCriticalSection();
    return ret;
}

static void psx_file_read_buffer_from_cd(psx_cd_file *f)
{
    CdlLOC loc;

    int position = CdPosToInt(&f->file.pos) + f->cursor_sectors;
    CdIntToPos(position, &loc);
    CdControl(CdlSetloc, &loc, 0);

    CdRead(1, (uint32_t *)f->read_buf, CdlModeSpeed);
    if (CdReadSync(0, 0) < 0) {
        Sys_Error("Failed to read from CD drive");
    }
    f->read_buf_is_valid = true;
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime(char const *path)
{
    // Hack, but OK for now...
    if (strcmp(path, "./id1/autoexec.cfg") == 0 || strcmp(path, "./id1/autoexec.cfg") == 0) {
        return 1;
    }
    printf("Sys_FileTime failed %s\n", path);
    return -1;
}

void Sys_mkdir(char const *path)
{
    printf("Sys_mkdir unsupported %s\n", path);
}

int Sys_FileOpenRead(char const *path, int *handle)
{
    psx_cd_file *f = NULL;
    char const *tmp = path;
    char pathbuf[64];
    size_t pathbuf_len = 0;

    if (*tmp == '.') {
        tmp += 1;
    }
    while (*tmp != 0) {
        if (*tmp == '/') {
            pathbuf_len = 0;
            // pathbuf[pathbuf_len++] = '\\';
        } else {
            pathbuf[pathbuf_len++] = toupper(*tmp);
        }
        tmp += 1;
    }
    pathbuf[pathbuf_len++] = 0;

    f = psx_file_alloc();
    if (f == NULL) {
        Sys_Error("All out of CD file handles..");
    }

    printf("Sys_FileOpenRead 0x%p %s\n", f, pathbuf);

    if (CdSearchFile(&f->file, pathbuf) == NULL) {
        printf("Failed to open file %s\n", path);
        f->allocated = false;
        goto error;
    }

    f->filename_hash = pq_hash(path, strlen(path));
    *handle = (int)f;
    return f->file.size;
error:
    *handle = -1;
    return -1;
}

int Sys_FileOpenWrite(char const *path)
{
    printf("Sys_FileOpenWrite unsupported %s", path);
    return 0;
}

int Sys_FileWrite(int handle, void const *src, int count)
{
    printf("Sys_FileWrite unsupported %d\n", count);
    return 0;
}

void Sys_FileClose(int handle)
{
    psx_cd_file *f = (psx_cd_file *)handle;

    if (!psx_file_is_valid(f)) {
        printf("Invalid file handle passed to close\n");
        return;
    }

    printf("Sys_FileClose 0x%p\n", f);

    f->allocated = false;
}

#define CD_FILE_SIZE_ROUND(sz) ((sz + 2047U) & 0xfffff800)

void Sys_FileSeek(int handle, int position)
{
    psx_cd_file *f = (psx_cd_file *)handle;

    printf("Sys_FileSeek 0x%p %u\n", f, position);

    if (!psx_file_is_valid(f)) {
        printf("Invalid file handle passed to seek\n");
        return;
    }

    f->cursor_sectors = position / PSX_CD_SECTOR_SIZE;
    f->cursor_bytes = position % PSX_CD_SECTOR_SIZE;
    f->read_buf_is_valid = false;

    printf("Seek %u, sec %u bytes %u, file len %u\n", position, f->cursor_sectors, f->cursor_bytes, f->file.size);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    size_t copied = 0;
    psx_cd_file *f = (psx_cd_file *)handle;

    if (!psx_file_is_valid(f)) {
        printf("Invalid file handle passed to read\n");
        return 0;
    }

    // TODO we do a double-read for the first partial chunk if this condition is false
    if (f->read_buf_is_valid && f->cursor_bytes + count < sizeof(f->read_buf)) {
        printf("Sys_FileRead buffered 0x%p %u\n", f, count);
        memcpy(dest, f->read_buf + f->cursor_bytes, count);
        f->cursor_bytes += count;
        if (f->cursor_bytes >= PSX_CD_SECTOR_SIZE) {
            f->cursor_sectors += 1;
            f->cursor_bytes -= PSX_CD_SECTOR_SIZE;
            f->read_buf_is_valid = false;
        }
        return count;
    }

    printf("Sys_FileRead drive 0x%p %u\n", f, count);
    while (copied < count) {
        psx_file_read_buffer_from_cd(f);

        // printf("read cursor %u %u sizes %u %u\n",
        // 	   f->cursor_sectors, f->cursor_bytes, copied, count);

        size_t copy_len = PSX_CD_SECTOR_SIZE - f->cursor_bytes;
        if ((copied + copy_len) > count) {
            copy_len = count - copied;
        }
        memcpy((uint8_t *)dest + copied, f->read_buf + f->cursor_bytes, copy_len);

        copied += copy_len;

        f->cursor_bytes += copy_len;
        if (f->cursor_bytes >= PSX_CD_SECTOR_SIZE) {
            f->cursor_sectors += 1;
            f->cursor_bytes -= PSX_CD_SECTOR_SIZE;
            f->read_buf_is_valid = false;
        }
    }

    printf("post read cursor %u %u\n", f->cursor_sectors, f->cursor_bytes);

    return copied;
}
