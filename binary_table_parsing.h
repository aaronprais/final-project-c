#ifndef BINARY_TABLE_PARSING_H
#define BINARY_TABLE_PARSING_H

#include "table.h"
#include "labels.h"

/*
 * binary_table_parsing.h
 * ----------------------
 * Constants for 10-bit words and A/R/E flags (ARE in bits 0..1).
 * Just a lil legend so we remember:
 *   - A_ARE: Absolute (00)
 *   - E_ARE: External (01)
 *   - R_ARE: Relocatable (10)
 */
#define TEN_BIT_MASK 0x3FF
#define E_ARE 0x1
#define R_ARE 0x2
#define A_ARE 0x0

/*
 * parse_table_to_binary
 * ---------------------
 * Walks the Table rows and encodes each one into its 10-bit machine word.
 * Uses Labels to resolve symbols when needed (like direct adressing).
 * returns TRUE on success, FALSE if any row failed (but still tries to keep going).
 */
int parse_table_to_binary(Table *table, Labels *labels, const char *src_filename);

#endif //BINARY_TABLE_PARSING_H
