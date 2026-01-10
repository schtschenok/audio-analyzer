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
#else
#include <dirent.h>
// #include <unistd.h>
#endif

#define NUM_THREADS 6

#include "load_file.c"
#include "parse_file_info.c"
#include "prepare_data.c"

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

// TODO: Reuse memory!!1
void* process_file(void* arg) {
    TracyCZoneN(ProcessFile, "Process File", true);
    const str_t* file_name = arg;

    if (file_name == NULL) {
        TracyCZoneEnd(IORead);
        int3();
        goto end;
    }

    // TODO: Do proper filename validation somewhere
    const char last_symbol = *(file_name->start + file_name->length - 1);
    if (last_symbol != 'v') {
        TracyCZoneEnd(IORead);
        // int3();
        goto end;
    }

    arena_t arena_temp_tl = arena_make(MB(8));

    loaded_file_t loaded_file = load_file(&arena_temp_tl, file_name);

    // TODO: Check if file loaded

    parsed_file_info_t parsed_file_info = parse_file_info(&loaded_file);

    // TODO: Check if file parsed

    prepared_data_t prepared_data = prepare_data(&parsed_file_info);

    unload_file(&loaded_file);

    TracyCZoneN(Analyze, "Analyze", true);

    const i64 sample_count_channel = prepared_data.size / prepared_data.channels;

    f64 max_db = 0;
    if (prepared_data.data_type == DATA_TYPE_INT) {
        u32 current_value;
        i64 max_value = 0;
        for (i64 channel = 0; channel < prepared_data.channels; channel++) {
            for (i64 sample = 0; sample < sample_count_channel; sample++) {
                const i64 sample_index = sample + channel * sample_count_channel;
                const i32 value = prepared_data.data[sample_index].i32;

                current_value = (u32)llabs((i64)value); // TODO: Optimize
                max_value = current_value > max_value ? current_value : max_value;
            }
        }
        max_db = i32_to_db((i32)max_value);
    } else {
        f32 current_value;
        f32 max_value = 0.0f;
        for (i64 channel = 0; channel < prepared_data.channels; channel++) {
            for (i64 sample = 0; sample < sample_count_channel; sample++) {
                const i64 sample_index = sample + channel * sample_count_channel;
                const f32 value = prepared_data.data[sample_index].f32;

                current_value = fabsf(value);
                max_value = current_value > max_value ? current_value : max_value;
            }
        }
        max_db = f32_to_db(max_value);
    }

    benchmark_int_accumulator += prepared_data.size;
    benchmark_float_accumulator += max_db;

    str_write(file_name, stdout, false);
    printf(" %.1f\n", max_db);

    TracyCZoneEnd(Analyze);

    benchmark_files_processed++;
    benchmark_bytes_processed += parsed_file_info.size;

unload_prepared_data:
    unload_prepared_data(&prepared_data);

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
