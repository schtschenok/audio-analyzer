#include <string.h>

#include "types.h"

#include "libs/tracy.h"

static inline i64 get_deinterleaved_index(const i64 byte_offset, const i64 channels, const i64 size, const i64 bytes_per_sample) {
    return (byte_offset / bytes_per_sample) % channels * (size / bytes_per_sample / channels) + (byte_offset / bytes_per_sample / channels);
}

prepared_data_t prepare_data(const parsed_file_info_t* parsed_file_info) {
    assert(parsed_file_info != NULL);
    assert(parsed_file_info->result == PARSE_FILE_INFO_SUCCESS);
    assert(parsed_file_info->loaded_file->data != NULL);
    assert(parsed_file_info->loaded_file->result == LOAD_FILE_SUCCESS);

    TracyCZoneN(PrepareData, "Prepare data", true);
    TracyCZoneN(Allocation, "Allocation", true);

    prepared_data_t prepared_data = { 0 };
    prepared_data.result = PREPARE_DATA_ERROR;

    if (parsed_file_info->result != PARSE_FILE_INFO_SUCCESS) {
        prepared_data.result = PREPARE_DATA_INVALID_PARSED_FILE_INFO;
        TracyCZoneEnd(Allocation);
        goto end;
    }

    if (parsed_file_info->loaded_file->result != LOAD_FILE_SUCCESS) {
        prepared_data.result = PREPARE_DATA_INVALID_LOADED_FILE;
        TracyCZoneEnd(Allocation);
        goto end;
    }

    const i64 size_to_allocate = parsed_file_info->size / (parsed_file_info->bits_per_sample / 8) * 4;

    data_value_t* data = memory_map_anonymous(size_to_allocate, true);

    if (data == NULL) {
        prepared_data.result = PREPARE_DATA_MAP_FAILED;
        TracyCZoneEnd(Allocation);
        goto end;
    }

    TracyCZoneEnd(Allocation);
    TracyCZoneN(Deinterleave, "Prepare data - Deinterleave", true);

    switch (parsed_file_info->bits_per_sample) {
    case 8: // int only
        TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 8-bit int", 64);
        for (i64 i = 0; i < parsed_file_info->size; i += sizeof(i8)) {
            const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 1);

            const i32 value = *(i8*)&parsed_file_info->data[i]; // NOLINT
            data[resulting_index].i32 = (i32)((u32)value << 24);
        }
        break;
    case 16: // int only
        TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 16-bit int", 64);
        for (i64 i = 0; i < parsed_file_info->size; i += sizeof(i16)) {
            const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 2);

            i16 temp_value;
            memcpy(&temp_value, &parsed_file_info->data[i], sizeof(i16));
            data[resulting_index].i32 = (i32)((u32)temp_value << 16);
        }
        break;
    case 24: // int only
        TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 24-bit int", 64);
        for (i64 i = 0; i < parsed_file_info->size; i += 3) {
            const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 3);

            const u8* bytes = (u8*)&parsed_file_info->data[i];

            i32 temp_value = bytes[0] | bytes[1] << 8 | bytes[2] << 16;

            if (temp_value & 0x800000) {
                temp_value |= (i32)0xFF000000;
            }

            data[resulting_index].i32 = (i32)((u32)temp_value << 8);
        }
        break;
    case 32: // int or float
        if (parsed_file_info->data_type == DATA_TYPE_INT) {
            TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 32-bit int", 64);
            for (i64 i = 0; i < parsed_file_info->size; i += sizeof(i32)) {
                const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 4);

                i32 temp_value;
                memcpy(&temp_value, &parsed_file_info->data[i], sizeof(i32));
                data[resulting_index].i32 = temp_value;
            }
            break;
        } else {
            TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 32-bit float", 64);
            for (i64 i = 0; i < parsed_file_info->size; i += sizeof(i32)) {
                const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 4);

                f32 temp_value;
                memcpy(&temp_value, &parsed_file_info->data[i], sizeof(f32));
                data[resulting_index].f32 = temp_value;
            }
            break;
        }
    case 64: // float only
        TracyCZoneName(Deinterleave, "Prepare data - Deinterleave 64-bit float", 64);
        for (i64 i = 0; i < parsed_file_info->size; i += sizeof(f64)) {
            const i64 resulting_index = get_deinterleaved_index(i, parsed_file_info->channels, parsed_file_info->size, 8);

            f64 temp_value;
            memcpy(&temp_value, &parsed_file_info->data[i], sizeof(f64));
            data[resulting_index].f32 = (f32)temp_value;
        }
        break;
    default:
        assert(false);
        break;
    }

    TracyCZoneEnd(Deinterleave);

    prepared_data.data = data;
    prepared_data.size = size_to_allocate / 4;
    prepared_data.data_type = parsed_file_info->data_type;
    prepared_data.channels = parsed_file_info->channels;
    prepared_data.sample_rate = parsed_file_info->sample_rate;
    prepared_data.file_name = parsed_file_info->loaded_file->file_name;
    prepared_data.result = PREPARE_DATA_SUCCESS;

end:
    TracyCZoneEnd(PrepareData);
    return prepared_data;
}

bool unload_prepared_data(prepared_data_t* prepared_data) {
    assert(prepared_data != NULL);
    assert(prepared_data->result == PREPARE_DATA_SUCCESS);

    if (prepared_data->result == PREPARE_DATA_SUCCESS) {
        memory_unmap_anonymous(prepared_data->data, prepared_data->size);
        prepared_data->data = NULL;
        return true;
    }
    return false;
}