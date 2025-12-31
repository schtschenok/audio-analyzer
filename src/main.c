#define CORE_IMPLEMENTATION
#include "base/core.h"

#define MEMORY_IMPLEMENTATION
#include "base/memory.h"

#define ARENA_IMPLEMENTATION
#include "base/arena.h"

#define STRING_IMPLEMENTATION
#include "base/string.h"

#define ARRAY_IMPLEMENTATION
#include "base/array.h"

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "libs/tracy.h"

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

#define NUM_THREADS 6

// TODO: Proper CLI args parsing
// TODO: For each found file - check if it's a file. If it is - check if it's a .wav file (extension + RIFF), if it is - save its path. If it's a dir - recurse over the directory and do the same.

static arena_t arena_global;
static arena_t arena_temp;

static i64 benchmark_files_processed = 0;
static i64 benchmark_bytes_processed = 0;
static i64 benchmark_start_time = 0;
static i64 benchmark_end_time = 0;
static i64 benchmark_int_accumulator = 0;
static f64 benchmark_float_accumulator = 0.0f;

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

i64 get_current_time_ms() {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000) / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

f64 i32_to_db(const i32 linear) {
    const f64 normalized = fabs((f64)linear) / (f64)INT32_MAX;
    return normalized > 0.0000000001 ? 20.0 * log10(normalized) : -200.0;
}

f64 i64_to_db(const i64 linear) {
    const f64 normalized = fabs((f64)linear) / (f64)INT64_MAX;
    return normalized > 0.0000000001 ? 20.0 * log10(normalized) : -200.0;
}

f64 f32_to_db(const f32 linear) {
    const f64 abs = fabs((f64)linear);
    return abs > 0.0000000001 ? 20.0 * log10(abs) : -200.0;
}

f64 f64_to_db(const f64 linear) {
    const f64 abs = fabs(linear);
    return abs > 0.0000000001 ? 20.0 * log10(abs) : -200.0;
}

i32 db_to_i32(const f64 db) {
    return (i32)(pow(10.0, db / 20.0) * (f64)INT32_MAX);
}

i64 db_to_i64(const f64 db) {
    return (i64)(pow(10.0, db / 20.0) * (f64)INT64_MAX);
}

f32 db_to_f32(const f64 db) {
    return (f32)pow(10.0, db / 20.0);
}

f64 db_to_f64(const f64 db) {
    return pow(10.0, db / 20.0);
}

void open_file(const str_t* file_name) {
}

static inline i64 get_deinterleaved_index(const i64 byte_offset, const i64 channels, const i64 size, const i64 bytes_per_sample) {
    return (byte_offset / bytes_per_sample) % channels * (size / bytes_per_sample / channels) + (byte_offset / bytes_per_sample / channels);
}

// TODO: Reuse memory!!1
void* process_file(void* arg) {
    TracyCZoneN(ProcessFile, "Process File", true);
    TracyCZoneN(IORead, "IO Read", true);
    const str_t* file_name = arg;

    if (file_name == NULL) {
        TracyCZoneEnd(IORead);
        int3();
        goto end;
    }

    arena_t arena_temp_tl = arena_make(MB(8));

    const c* file_name_cstr = str_to_cstr(&arena_temp_tl, file_name);
    const int fd = open(file_name_cstr, O_RDONLY);

    if (fd == -1) {
        TracyCZoneEnd(IORead);
        int3();
        goto end;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        TracyCZoneEnd(IORead);
        int3();
        goto close_file;
    }

    u8* file = memory_map_file(fd, sb.st_size, true, false, true);

    if (file == NULL) {
        TracyCZoneEnd(IORead);
        int3();
        goto close_file;
    }

    if (sb.st_size < 12) {
        TracyCZoneEnd(IORead);
        int3();
        goto unmap_file;
    }

    TracyCZoneEnd(IORead);
    TracyCZoneN(ParseHeaders, "Parse Headers", true);
    wave_riff_header_t riff_header = {};
    memcpy(&riff_header, file, 12);

    const u8* fmt_chunk_in_file = file + 12;

    if (fmt_chunk_in_file - file + 8 > sb.st_size) {
        TracyCZoneEnd(ParseHeaders);

        printf("fmt chunk is out of bounds, invalid file\n");
        goto unmap_file;
    }

    while (memcmp(fmt_chunk_in_file, "fmt ", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, fmt_chunk_in_file + 4, sizeof(u32));
        fmt_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (fmt_chunk_in_file > file + sb.st_size - sizeof(wave_fmt_chunk_t)) {
            TracyCZoneEnd(ParseHeaders);
            printf("Couldn't find fmt string\n");
            int3();
            goto unmap_file;
        }
    }

    wave_fmt_chunk_t fmt_chunk = {};
    memcpy(&fmt_chunk, fmt_chunk_in_file, sizeof(wave_fmt_chunk_t));

    bool is_int;
    if (fmt_chunk.format_type == 1) {
        is_int = true;
    } else if (fmt_chunk.format_type == 3) {
        is_int = false;
    } else if (fmt_chunk.format_type == 65534) {
        const u8* guid_ptr = fmt_chunk_in_file + 8 + 24;
        u32 guid_four_bytes;
        memcpy(&guid_four_bytes, guid_ptr, sizeof(u32));
        if (guid_four_bytes == 1) {
            is_int = true;
        } else if (guid_four_bytes == 3) {
            is_int = false;
        } else {
            int3();
            TracyCZoneEnd(ParseHeaders);
            goto unmap_file;
        }
    } else {
        int3();
        TracyCZoneEnd(ParseHeaders);
        goto unmap_file;
    }

    const u8* data_chunk_in_file = fmt_chunk_in_file + 8 + fmt_chunk.fmt_size + (fmt_chunk.fmt_size & 1);

    if (data_chunk_in_file - file + 8 > sb.st_size) {
        TracyCZoneEnd(ParseHeaders);
        printf("Data chunk is out of bounds, invalid file\n");
        goto unmap_file;
    }

    while (memcmp(data_chunk_in_file, "data", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, data_chunk_in_file + 4, sizeof(u32));
        data_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (data_chunk_in_file > file + sb.st_size - sizeof(wave_generic_chunk_t)) {
            TracyCZoneEnd(ParseHeaders);
            printf("Couldn't find data string\n");
            int3();
            goto unmap_file;
        }
    }

    wave_generic_chunk_t data_chunk = {};
    memcpy(&data_chunk, data_chunk_in_file, sizeof(wave_generic_chunk_t));

    const i64 remaining_size_after_data = (sb.st_size - (data_chunk_in_file + 8 - file)) - data_chunk.size;

    if (remaining_size_after_data < 0) {
        TracyCZoneEnd(ParseHeaders);
        printf("Data size is not valid\n");
        int3();
        goto unmap_file;
    }

    // printf("RIFF: %.4s, Size: %u, WAVE: %.4s, fmt: %.3s, fmt_size: %u, format_type: %u, channels: %u, sample_rate: %u, byterate: %u, block_align: %u, bits_per_sample: %u, data: %.4s, data_size: %u, data_size_difference: %lld\n",
    //        riff_header.riff_marker,
    //        riff_header.overall_size,
    //        riff_header.wave_marker,
    //        fmt_chunk.fmt_marker,
    //        fmt_chunk.fmt_size,
    //        fmt_chunk.format_type,
    //        fmt_chunk.channels,
    //        fmt_chunk.sample_rate,
    //        fmt_chunk.byterate,
    //        fmt_chunk.block_align,
    //        fmt_chunk.bits_per_sample,
    //        data_chunk.marker,
    //        data_chunk.size,
    //        remaining_size_after_data);

    TracyCZoneEnd(ParseHeaders);
    TracyCZoneN(Allocation, "Allocation", true);

    const u8* original_data = data_chunk_in_file + 8;

    i64 data_size = data_chunk.size / (fmt_chunk.block_align / fmt_chunk.channels) * 4;

    u8* data = memory_map_anonymous(data_size, true);

    if (data == NULL) {
        TracyCZoneEnd(Allocation);
        int3();
        goto unmap_file;
    }

    TracyCZoneEnd(Allocation);
    TracyCZoneN(Deinterleave, "Deinterleave", true);

    i32* data_i = (i32*)data;
    f32* data_f = (f32*)data;

    switch (fmt_chunk.bits_per_sample) {
    case 8: // int only
        TracyCZoneName(Deinterleave, "Deinterleave 8-bit int", 22);
        if (!is_int) {
            int3();
            break;
        }

        for (i64 i = 0; i < data_chunk.size; i += sizeof(i8)) {
            const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 1);

            const i32 value = *(i8*)&original_data[i]; // NOLINT
            data_i[resulting_index] = (i32)((u32)value << 24);
        }
        break;
    case 16: // int only
        TracyCZoneName(Deinterleave, "Deinterleave 16-bit int", 23);
        if (!is_int) {
            int3();
            break;
        }

        for (i64 i = 0; i < data_chunk.size; i += sizeof(i16)) {
            const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 2);

            i16 temp_value;
            memcpy(&temp_value, &original_data[i], sizeof(i16));
            data_i[resulting_index] = (i32)((u32)temp_value << 16);
        }
        break;
    case 24: // int only
        TracyCZoneName(Deinterleave, "Deinterleave 24-bit int", 23);
        if (!is_int) {
            int3();
            break;
        }

        for (i64 i = 0; i < data_chunk.size; i += 3) {
            const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 3);

            u8* bytes = (u8*)&original_data[i];
            i32 temp_value;

            temp_value = bytes[0] | bytes[1] << 8 | bytes[2] << 16;

            if (temp_value & 0x800000) {
                temp_value |= (i32)0xFF000000;
            }

            data_i[resulting_index] = (i32)((u32)temp_value << 8);
        }
        break;
    case 32: // int or float
        if (is_int) {
            TracyCZoneName(Deinterleave, "Deinterleave 32-bit int", 23);
            for (i64 i = 0; i < data_chunk.size; i += sizeof(i32)) {
                const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 4);

                i32 temp_value;
                memcpy(&temp_value, &original_data[i], sizeof(i32));
                data_i[resulting_index] = temp_value;
            }
            break;
        } else {
            TracyCZoneName(Deinterleave, "Deinterleave 32-bit float", 25);
            for (i64 i = 0; i < data_chunk.size; i += sizeof(i32)) {
                const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 4);

                f32 temp_value;
                memcpy(&temp_value, &original_data[i], sizeof(f32));
                data_f[resulting_index] = temp_value;
            }
            break;
        }
    case 64: // float only
        TracyCZoneName(Deinterleave, "Deinterleave 64-bit float", 25);
        if (is_int) {
            int3();
            break;
        }

        for (i64 i = 0; i < data_chunk.size; i += sizeof(f64)) {
            const i64 resulting_index = get_deinterleaved_index(i, fmt_chunk.channels, data_chunk.size, 8);

            f64 temp_value;
            memcpy(&temp_value, &original_data[i], sizeof(f64));
            data_f[resulting_index] = (f32)temp_value;
        }
        break;
    default:
        int3();
        break;
    }

    TracyCZoneEnd(Deinterleave);

    TracyCZoneN(Analyze, "Analyze", true);

    const i64 sample_count_total = data_size / 4;
    const i64 channels = fmt_chunk.channels;
    const i64 sample_count_channel = sample_count_total / channels;

    f64 max_db = 0;
    if (is_int) {
        u32 current_value;
        i64 max_value = 0;
        for (i64 channel = 0; channel < channels; channel++) {
            for (i64 sample = 0; sample < sample_count_channel; sample++) {
                const i64 sample_index = sample + channel * sample_count_channel;
                const i32 value = data_i[sample_index];

                current_value = (u32)llabs((i64)value); // TODO: Optimize
                max_value = current_value > max_value ? current_value : max_value;
            }
        }
        max_db = i32_to_db((i32)max_value);
    } else {
        f32 current_value;
        f32 max_value = 0.0f;
        for (i64 channel = 0; channel < channels; channel++) {
            for (i64 sample = 0; sample < sample_count_channel; sample++) {
                const i64 sample_index = sample + channel * sample_count_channel;
                const f32 value = data_f[sample_index];

                current_value = fabsf(value);
                max_value = current_value > max_value ? current_value : max_value;
            }
        }
        max_db = f32_to_db(max_value);
    }

    benchmark_int_accumulator += sample_count_total;
    benchmark_float_accumulator += max_db;

    // str_write(file_name, stdout, true);
    // printf(": %.3f\n", max_db);

    TracyCZoneEnd(Analyze);

    benchmark_files_processed++;
    benchmark_bytes_processed += data_chunk.size;

unmap_data:
    memory_unmap_anonymous(data, data_size);
unmap_file:
    memory_unmap_file(file, sb.st_size);
close_file:
    close(fd);

    arena_clear(&arena_temp_tl);

end:
    TracyCZoneEnd(ProcessFile);
    return NULL;
}

int main(const int argc, char* argv[]) {
    if (argc < 2) {
        printf("%s\n", "Please supply at least one argument.");
        exit(1);
    }

    arena_global = arena_make(GB(1));

    c* test = arena_alloc(&arena_global, 10);
    test[0] = 'a';

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

    benchmark_start_time = get_current_time_ms();

    // TODO: REDO
    for (u32 i = 0; i < get_array_length(paths); i++) {
        struct stat path_stat;
        const char* path_cstr = str_to_cstr(&arena_temp, &paths[i]);

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
                }
            }
            closedir(dir);
        } else if (S_ISREG(path_stat.st_mode)) {
            process_file(&paths[i]);
        }

        arena_clear(&arena_temp);
    }

    benchmark_end_time = get_current_time_ms();

    const f64 benchmark_seconds = (f64)(benchmark_end_time - benchmark_start_time) / 1000.0;
    const f64 benchmark_megabytes = (f64)benchmark_bytes_processed / 1024.0 / 1024.0;
    printf("File count: %lld\n", (long long)benchmark_files_processed);
    printf("Time: %.3fs\n", benchmark_seconds);
    printf("Size: %.3fMB\n", benchmark_megabytes);
    printf("Speed: %.3f MB/s\n", benchmark_megabytes / benchmark_seconds);

    printf("Int accumulator: %lld\n", (long long)benchmark_int_accumulator);
    printf("Float accumulator: %.3f\n", benchmark_float_accumulator);
}
