#ifndef FILES_H
#define FILES_H

#include <stdint.h>

struct psx_cd_file_s;

/**
 * Allocate a new file handle for a PSX file.
 * @return Pointer to available file handle on success,
 *         NULL on error.
 */
psx_cd_file_s * psx_file_alloc();

/**
 * Duplicate a previously opened file handle.
 * This copies over the internal data structures, with the cursor data structures reset.
 * @param filename_hash Hash of the filename
 * @return Pointer to available file handle on success,
 *         NULL on error (file not found, no available handles).
 */
void * psx_file_duplicate_handle(uint32_t filename_hash);

#endif //FILES_H
