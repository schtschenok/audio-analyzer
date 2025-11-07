#include <dirent.h>
#include <fcntl.h>
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

#define NUM_THREADS 6

// TODO: Proper CLI args parsing
// TODO: For each found file - check if it's a file. If it is - check if it's a .wav file (extension + RIFF), if it is - save its path. If it's a dir - recurse over the directory and do the same.

static arena_t arena_global;
static arena_t arena_temp;

typedef struct {
    c riff[4];
    u32 overall_size;
    c wave[4];
    c fmt_chunk_marker[4];
    u32 length_of_fmt;
    u16 format_type;
    u16 channels;
    u32 sample_rate;
    u32 byterate;
    u16 block_align;
    u16 bits_per_sample;
} wave_file_header_t;

typedef struct {
    c riff_marker[4];
    u32 overall_size;
    c wave_marker[4];
} wave_riff_header_t;

typedef struct {
    c fmt_marker[4];
    u32 fmt_size;
    u16 format_type;
    u16 channels;
    u32 sample_rate;
    u32 byterate;
    u16 block_align;
    u16 bits_per_sample;
} wave_fmt_chunk_t;

typedef struct {
    c marker[4];
    u32 size;
} wave_generic_chunk_t;

void* process_file(void* arg) {
    const str_t* file_name = arg;

    if (file_name == NULL) {
        int3();
        return NULL;
    }

    arena_t arena_temp_tl = arena_make(MB(8));

    const c* file_name_cstr = str_to_cstr(&arena_temp_tl, *file_name);
    const int fd = open(file_name_cstr, O_RDONLY);

    if (fd == -1) {
        int3();
        return NULL;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        int3();
        goto close_file;
    }

    u8* file = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // TODO: Benchmark with | MAP_POPULATE

    if (file == MAP_FAILED) {
        int3();
        goto close_file;
    }

    if (sb.st_size < 12) {
        int3();
        goto unmap_file;
    }

    wave_riff_header_t riff_header = {};
    memcpy(&riff_header, file, 12);

    const u8* fmt_chunk_in_file = file + 12;
    while (memcmp(fmt_chunk_in_file, "fmt ", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, fmt_chunk_in_file + 4, sizeof(u32));
        fmt_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (fmt_chunk_in_file > file + sb.st_size - sizeof(wave_fmt_chunk_t)) {
            printf("Couldn't find fmt string\n");
            int3();
            goto unmap_file;
        }
    }

    wave_fmt_chunk_t fmt_chunk = {};
    memcpy(&fmt_chunk, fmt_chunk_in_file, sizeof(wave_fmt_chunk_t));

    const u8* data_chunk_in_file = fmt_chunk_in_file + 8 + fmt_chunk.fmt_size + (fmt_chunk.fmt_size & 1);
    while (memcmp(data_chunk_in_file, "data", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, data_chunk_in_file + 4, sizeof(u32));
        data_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (data_chunk_in_file > file + sb.st_size - sizeof(wave_generic_chunk_t)) {
            printf("Couldn't find data string\n");
            int3();
            goto unmap_file;
        }
    }

    wave_generic_chunk_t data_chunk = {};
    memcpy(&data_chunk, data_chunk_in_file, sizeof(wave_generic_chunk_t));

    const i64 remaining_size_after_data = (sb.st_size - (data_chunk_in_file + 8 - file)) - data_chunk.size;

    if (remaining_size_after_data < 0) {
        printf("Data size is not valid\n");
        int3();
        goto unmap_file;
    }

    printf("RIFF: %.4s, Size: %u, WAVE: %.4s, fmt: %.4s, fmt_size: %u, format_type: %u, channels: %u, sample_rate: %u, byterate: %u, block_align: %u, bits_per_sample: %u, data: %.4s, data_size: %u, data_size_difference: %ld\n",
           riff_header.riff_marker,
           riff_header.overall_size,
           riff_header.wave_marker,
           fmt_chunk.fmt_marker,
           fmt_chunk.fmt_size,
           fmt_chunk.format_type,
           fmt_chunk.channels,
           fmt_chunk.sample_rate,
           fmt_chunk.byterate,
           fmt_chunk.block_align,
           fmt_chunk.bits_per_sample,
           data_chunk.marker,
           data_chunk.size,
           remaining_size_after_data);

    const u8* original_data = data_chunk_in_file + 8;

    u32 data_size = align_size(data_chunk.size, getpagesize());
    switch (fmt_chunk.bits_per_sample) {
    case 24:
        data_size = data_chunk.size / 3 * 4;
        break;
    default:
        data_size = data_chunk.size;
    }

    u8* data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        int3();
        goto unmap_file;
    }

    i16* data_i16 = (i16*)data;
    i32* data_i32 = (i32*)data;
    f32* data_f32 = (f32*)data;
    f64* data_f64 = (f64*)data;

    // if (fmt_chunk.channels != 4 || fmt_chunk.format_type != 65534 || fmt_chunk.bits_per_sample != 16) {
    //     goto unmap_file;
    // }

    switch (fmt_chunk.format_type) {
    case 1:
    case 65534:
        switch (fmt_chunk.bits_per_sample) {
        case 16:
            for (u32 i = 0; i < data_chunk.size; i += sizeof(i16)) {
                const u32 resulting_index = (i / sizeof(i16) / fmt_chunk.channels) + (data_chunk.size / sizeof(i16) / fmt_chunk.channels) * (i / sizeof(i16) % fmt_chunk.channels);
                data_i16[resulting_index] = original_data[i];
            }
            break;
        case 24: // 32
            for (u32 i = 0; i < data_chunk.size; i += 3) {
                const u32 resulting_index = (i / 3 / fmt_chunk.channels) + (data_chunk.size / 3 / fmt_chunk.channels) * (i / 3 % fmt_chunk.channels);
                i32 widened_24bit_value;

                u8* bytes = (u8*)&original_data[i];
                i32 value;

                value = bytes[0] | bytes[1] << 8 | bytes[2] << 16;

                if (value & 0x800000) {
                    value |= (i32)0xFF000000;
                }

                data_i32[resulting_index] = value;
            }
        case 32:
            for (u32 i = 0; i < data_chunk.size; i += sizeof(i32)) {
                const u32 resulting_index = (i / sizeof(i32) / fmt_chunk.channels) + (data_chunk.size / sizeof(i32) / fmt_chunk.channels) * (i / sizeof(i32) % fmt_chunk.channels);
                data_i32[resulting_index] = original_data[i];
            }
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (fmt_chunk.bits_per_sample) {
        case 32:
            for (u32 i = 0; i < data_chunk.size; i += sizeof(f32)) {
                const u32 resulting_index = (i / sizeof(f32) / fmt_chunk.channels) + (data_chunk.size / sizeof(f32) / fmt_chunk.channels) * (i / sizeof(f32) % fmt_chunk.channels);
                data_f32[resulting_index] = original_data[i];
            }
            break;
        case 64:
            for (u32 i = 0; i < data_chunk.size; i += sizeof(f64)) {
                const u32 resulting_index = (i / sizeof(f64) / fmt_chunk.channels) + (data_chunk.size / sizeof(f64) / fmt_chunk.channels) * (i / sizeof(f64) % fmt_chunk.channels);
                data_f64[resulting_index] = original_data[i];
            }
            break;
        default:
            break;
        }
    default:
        goto unmap_data;
    }

unmap_data:
    munmap(data, data_size);
unmap_file:
    munmap(file, sb.st_size);
close_file:
    close(fd);

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

    u32 file_counter = 0;

    // TODO: REDO
    for (u32 i = 0; i < get_array_length(paths); i++) {
        struct stat path_stat;
        const char* path_cstr = str_to_cstr(&arena_temp, paths[i]);

        if (stat(path_cstr, &path_stat) != 0) {
            printf("Failed to get stats for path: %s\n", path_cstr);
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            DIR* dir = opendir(path_cstr);
            if (dir == NULL) {
                printf("Failed to open directory: %s\n", path_cstr);
                continue;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;

                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", path_cstr, entry->d_name);

                str_t entry_path = str_from_cstr(&arena_global, full_path);
                struct stat entry_stat;
                if (stat(full_path, &entry_stat) != 0) {
                    printf("Failed to get stats for path: %s\n", full_path);
                    continue;
                }
                if (entry_stat.st_mode & S_IFREG) {
                    process_file(&entry_path);
                    file_counter++;
                }
            }
            closedir(dir);
        } else if (S_ISREG(path_stat.st_mode)) {
            process_file(&paths[i]);
            file_counter++;
        }

        arena_clear(&arena_temp);
    }

    printf("%u", file_counter);

    // pthread_t* threads = array_from_size(pthread_t, &arena_global, NUM_THREADS);
    //
    // for (u64 i = 0; i < get_array_length(paths) && i < get_array_capacity(threads); i++) {
    //     pthread_create(&threads[i], NULL, process_file, &paths[i]);
    //     get_array_header(threads)->length++;
    // }
    //
    // for (u64 i = 0; i < get_array_length(threads); i++) {
    //     pthread_join(threads[i], NULL);
    // }
}
