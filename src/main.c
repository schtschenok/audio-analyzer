#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define STRING_IMPLEMENTATION
#include "base/string.h"

#define ARRAY_IMPLEMENTATION
#include "base/array.h"

#define DR_WAV_IMPLEMENTATION
#include "libs/dr_wav.h"

#define NUM_THREADS 6

// TODO: Proper CLI args parsing
// TODO: For each found file - check if it's a file. If it is - check if it's a .wav file (extension + RIFF), if it is - save its path. If it's a dir - recurse over the directory and do the same.

static arena_t arena_global;
static arena_t arena_temp;

void* process_file(void* arg) {
    str_t* file_name = arg;

    if (file_name == NULL) {
        return NULL;
    }

    arena_t arena_temp_tl = arena_make(MB(8));

    char* file_name_cstr = str_to_cstr(&arena_temp_tl, *file_name);
    int fd = open(file_name_cstr, O_RDONLY);

    if (fd == -1) {
        return NULL;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        goto close_file;
    }

    void* file = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // TODO: Benchmark with | MAP_POPULATE
    if (file == MAP_FAILED) {
        goto close_file;
    }

    u32 channels;
    u32 samplerate;
    u64 frame_count;
    f32* data = drwav_open_memory_and_read_pcm_frames_f32(file, sb.st_size, &channels, &samplerate, (drwav_uint64*)&frame_count, NULL);

    printf("%s, size: %ld\n, channels: %d, samplerate: %d, frame_count: %ld\n", file_name_cstr, sb.st_size, channels, samplerate, frame_count);

unmap_file:
    munmap(file, sb.st_size);
close_file:
    close(fd);
clean_temp_arena:
    arena_clear(&arena_temp_tl);

    return NULL;
}

int main(const int argc, char* argv[]) {
    if (argc < 2) {
        printf("%s\n", "Please supply at least one argument.");
        exit(1);
    }

    arena_global = arena_make(GB(1));

    arena_temp = arena_make(MB(4));

    str_t* paths = array_from_size(str_t, &arena_global, argc - 1);
    char* absolute_path_buffer = arena_alloc(&arena_temp, PATH_MAX);

    for (u64 i = 1; i < argc; i++) {
        if (realpath(argv[i], absolute_path_buffer)) {
            const str_t absolute_path = str_from_cstr(&arena_global, absolute_path_buffer);
            paths[i - 1] = absolute_path;
            get_array_header(paths)->length++; // TODO: Add generic add/get functions to array.h
        } else {
            printf("Couldn't find real path for argument \"%s\"\n", argv[i]);
        }
    }
    arena_clear(&arena_temp);

    pthread_t* threads = array_from_size(pthread_t, &arena_global, NUM_THREADS);

    for (u64 i = 0; i < get_array_length(paths) && i < get_array_capacity(threads); i++) {
        pthread_create(&threads[i], NULL, process_file, &paths[i]);
        get_array_header(threads)->length++;
    }

    for (u64 i = 0; i < get_array_length(threads); i++) {
        pthread_join(threads[i], NULL);
    }
}
