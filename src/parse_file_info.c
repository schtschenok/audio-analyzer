#include "types.h"

#include "libs/tracy.h"

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

parsed_file_info_t parse_file_info(loaded_file_t* loaded_file) {
    assert(loaded_file != NULL);
    assert(loaded_file->result == LOAD_FILE_SUCCESS);

    TracyCZoneN(ParseFileInfo, "Parse File Info", true);

    parsed_file_info_t parsed_file_info = { 0 };
    parsed_file_info.result = PARSE_FILE_INFO_ERROR;

    wave_riff_header_t riff_header = {};
    memcpy(&riff_header, loaded_file->data, 12);

    const u8* fmt_chunk_in_file = loaded_file->data + 12;

    if (fmt_chunk_in_file - loaded_file->data + 8 > loaded_file->data_size) {
        parsed_file_info.result = PARSE_FILE_INFO_INVALID_FMT;
        goto end;
    }

    while (memcmp(fmt_chunk_in_file, "fmt ", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, fmt_chunk_in_file + 4, sizeof(u32));
        fmt_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (fmt_chunk_in_file > loaded_file->data + loaded_file->data_size - sizeof(wave_fmt_chunk_t)) {
            parsed_file_info.result = PARSE_FILE_INFO_INVALID_FMT;
            goto end;
        }
    }

    wave_fmt_chunk_t fmt_chunk = {};
    memcpy(&fmt_chunk, fmt_chunk_in_file, sizeof(wave_fmt_chunk_t));

    if (fmt_chunk.format_type == 1) {
        parsed_file_info.data_type = DATA_TYPE_INT;
    } else if (fmt_chunk.format_type == 3) {
        parsed_file_info.data_type = DATA_TYPE_FLOAT;
    } else if (fmt_chunk.format_type == 65534) {
        const u8* guid_ptr = fmt_chunk_in_file + 8 + 24;
        u32 guid_four_bytes;
        memcpy(&guid_four_bytes, guid_ptr, sizeof(u32));
        if (guid_four_bytes == 1) {
            parsed_file_info.data_type = DATA_TYPE_INT;
        } else if (guid_four_bytes == 3) {
            parsed_file_info.data_type = DATA_TYPE_FLOAT;
        } else {
            parsed_file_info.result = PARSE_FILE_INFO_UNSUPPORTED_FORMAT;
            goto end;
        }
    } else {
        parsed_file_info.result = PARSE_FILE_INFO_UNSUPPORTED_FORMAT;
        goto end;
    }

    const u8* data_chunk_in_file = fmt_chunk_in_file + 8 + fmt_chunk.fmt_size + (fmt_chunk.fmt_size & 1);

    if (data_chunk_in_file - loaded_file->data + 8 > loaded_file->data_size) {
        parsed_file_info.result = PARSE_FILE_INFO_INVALID_DATA;
        goto end;
    }

    while (memcmp(data_chunk_in_file, "data", 4) != 0) {
        u32 next_chunk_size;
        memcpy(&next_chunk_size, data_chunk_in_file + 4, sizeof(u32));
        data_chunk_in_file += 8 + next_chunk_size + (next_chunk_size & 1);
        if (data_chunk_in_file > loaded_file->data + loaded_file->data_size - sizeof(wave_generic_chunk_t)) {
            parsed_file_info.result = PARSE_FILE_INFO_INVALID_DATA;
            goto end;
        }
    }

    wave_generic_chunk_t data_chunk = {};
    memcpy(&data_chunk, data_chunk_in_file, sizeof(wave_generic_chunk_t));

    const i64 remaining_size_after_data = (loaded_file->data_size - (data_chunk_in_file + 8 - loaded_file->data)) - data_chunk.size;

    if (remaining_size_after_data < 0) {
        parsed_file_info.result = PARSE_FILE_INFO_INVALID_DATA;
        goto end;
    }

    parsed_file_info.loaded_file = loaded_file;
    parsed_file_info.data = data_chunk_in_file + 8;
    parsed_file_info.size = data_chunk.size;
    parsed_file_info.bits_per_sample = fmt_chunk.bits_per_sample;
    parsed_file_info.channels = fmt_chunk.channels;
    parsed_file_info.sample_rate = fmt_chunk.sample_rate;
    parsed_file_info.result = PARSE_FILE_INFO_SUCCESS;

end:
    TracyCZoneEnd(ParseFileInfo);
    return parsed_file_info;
}