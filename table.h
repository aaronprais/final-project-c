#pragma once
#include "util.h"
#ifndef TABLE_H
#define TABLE_H

typedef struct {
    char label[MAX_LABEL_LEN];
    unsigned int decimal_address;
    CommandType command : 5;
    unsigned int is_command_line : 1;
    char operands_string[MAX_OPERAND_LEN];
    unsigned int binary_machine_code : 10;
} row;

// Add these to your table.h file:
typedef struct {
    row *data;
    int size;
    int capacity;
} Table;

Table* create_table();
void free_table(Table *tbl);
void ensure_capacity(Table *tbl);
void add_row(Table *tbl, const char *label, CommandType cmd, int is_cmd_line, const char *operands, unsigned int binary_code);
void edit_row(Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line, const char *operands, unsigned int binary_code);
void insert_row(Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line, const char *operands, unsigned int binary_code);
row* get_row(Table *tbl, int index);
void reset_addresses(Table *tbl, unsigned int offset);
void print_table(Table *tbl);
#endif //TABLE_H
