#pragma once
#include "base/arena.h"
#include "base/core.h"
#include "base/memory.h"
#include "base/string.h"

typedef enum {
    LOAD_FILE_SUCCESS,
    LOAD_FILE_ERROR,
    LOAD_FILE_INVALID_FILE,
    LOAD_FILE_MAP_FAILED,
    LOAD_FILE_TOO_SMALL
} load_file_result_t;

typedef struct loaded_file_s {
    u8* data;
    i64 data_size;
    const str_t* file_name;
    load_file_result_t result;
} loaded_file_t;

typedef enum {
    DATA_TYPE_INT,
    DATA_TYPE_FLOAT
} data_type_t;

typedef enum {
    PARSE_FILE_INFO_SUCCESS,
    PARSE_FILE_INFO_ERROR,
    PARSE_FILE_INFO_INVALID_RIFF,
    PARSE_FILE_INFO_INVALID_FMT,
    PARSE_FILE_INFO_INVALID_DATA,
    PARSE_FILE_INFO_UNSUPPORTED_FORMAT
} parse_file_info_result_t;

typedef struct {
    loaded_file_t* loaded_file;
    const u8* data;
    i64 size;
    i64 bits_per_sample;
    data_type_t data_type;
    i64 channels;
    i64 sample_rate;
    parse_file_info_result_t result;
} parsed_file_info_t;

typedef enum {
    PREPARE_DATA_SUCCESS,
    PREPARE_DATA_ERROR,
    PREPARE_DATA_INVALID_PARSED_FILE_INFO,
    PREPARE_DATA_INVALID_LOADED_FILE,
    PREPARE_DATA_MAP_FAILED
} prepare_data_result_t;

typedef union {
    i32 i32;
    f32 f32;
} data_value_t;

typedef struct {
    data_value_t* data;
    i64 size;
    data_type_t data_type;
    i64 channels;
    i64 sample_rate;
    const str_t* file_name;
    prepare_data_result_t result;
    } prepared_data_t;
