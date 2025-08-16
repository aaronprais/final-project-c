#ifndef BINARY_TABLE_PARSING_H
#define BINARY_TABLE_PARSING_H

#include "table.h"
#include "labels.h"

#define TEN_BIT_MASK 0x3FF
#define E_ARE 0x1
#define R_ARE 0x2
#define A_ARE 0x0

int parse_table_to_binary(Table *table, Labels *labels, const char *src_filename);

#endif //BINARY_TABLE_PARSING_H
