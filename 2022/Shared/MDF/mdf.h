#ifndef MDF_H
#define MDF_H

#include <stdint.h>

// Based on the vector MDF 3.3.1 open specification

struct mdf_block_idblock {
    char file_id[8];
    char format_id[8];
    char program_id[8];
    uint16_t byte_order;
    uint16_t float_format;
    uint16_t version_number;
    uint16_t code_page_number;
    char reserved_1[2];
    char reserved_2[26];
};

#endif