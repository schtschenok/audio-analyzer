#include "types.h"

#include "libs/tracy.h"

#include <fcntl.h>

#ifdef _WIN32
#include "libs/dirent.h"
#include <io.h>
#define realpath(N, R) _fullpath((R), (N), PATH_MAX)
#define open(N, R) _open(N, R)
#define close(N) _close(N)
#else
#include <dirent.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

loaded_file_t load_file(arena_t* arena, const str_t* file_name) {
    assert(arena != NULL);
    assert(file_name != NULL);

    TracyCZoneN(IORead, "IO Read", true);

    loaded_file_t loaded_file = { 0 };
    loaded_file.result = LOAD_FILE_ERROR;

    if (file_name->length == 0) {
        loaded_file.result = LOAD_FILE_INVALID_FILE;
        goto end;
    }

    const c* file_name_cstr = str_to_cstr(arena, file_name);
    const int fd = open(file_name_cstr, O_RDONLY);

    if (fd == -1) {
        loaded_file.result = LOAD_FILE_INVALID_FILE;
        goto end;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        loaded_file.result = LOAD_FILE_INVALID_FILE;
        goto close_file;
    }

    if (sb.st_size < 128) {
        loaded_file.result = LOAD_FILE_TOO_SMALL;
        goto close_file;
    }

    u8* file = memory_map_file(fd, sb.st_size, true, false, true);

    if (file == NULL) {
        loaded_file.result = LOAD_FILE_MAP_FAILED;
        goto close_file;
    }

    loaded_file.file_name = file_name;
    loaded_file.data_size = sb.st_size;
    loaded_file.data = file;
    loaded_file.result = LOAD_FILE_SUCCESS;

close_file:
    close(fd);
end:
    TracyCZoneEnd(IORead);
    return loaded_file;
}

bool unload_file(loaded_file_t* loaded_file) {
    assert(loaded_file != NULL);
    assert(loaded_file->result == LOAD_FILE_SUCCESS);

    if (loaded_file->result == LOAD_FILE_SUCCESS) {
        memory_unmap_file(loaded_file->data, loaded_file->data_size);
        loaded_file->data = NULL;
        return true;
    }
    return false;
}