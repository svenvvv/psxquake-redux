#include "psx/io.h"

#include "psx/files.h"
#include "sys.h"
#include "util/hashlib.h"

#include <stdio.h>
#include <string.h>

FILE *fopen(const char *filename, const char *mode)
{
    size_t filename_len = strlen(filename);
    uint32_t search_hash = pq_hash(filename, filename_len);
    printf("fopen %s %s\n", filename, mode);

    if (strchr(mode, 'r')) {
        return psx_file_duplicate_handle(search_hash);
    }

    printf("Unsupported mode in fopen: %s\n", mode);
    return 0;
}

int fscanf(FILE *stream, const char *format, ...)
{
    printf("fscanf\n");
    return 0;
}

int fprintf(FILE *stream, const char *format, ...)
{
    return 0;
}

int fclose(FILE *stream)
{
    Sys_FileClose((int)stream);
    return 0;
}

int feof(FILE *stream)
{
    printf("feof\n");
    return 0;
}

int fseek(FILE *stream, long offset, int whence)
{
    printf("fseek %u %u\n", offset, whence);
    Sys_FileSeek((int)stream, offset);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    printf("fread %u %u\n", size, nmemb);
    return Sys_FileRead((int)stream, ptr, size * nmemb);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return 0;
}

int fflush(FILE *stream)
{
    return 0;
}

int unlink(char const *pathname)
{
    return 0;
}

int fgetc(FILE *stream)
{
    int err;
    uint8_t ret;

    printf("fgetc\n");

    err = Sys_FileRead((int)stream, &ret, sizeof(ret));
    if (err != sizeof(ret)) {
        return EOF;
    }

    return ret;
}
