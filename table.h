#ifndef TABLE_H
#define TABLE_H
#include "util.h"

typedef struct {
    char label[MAX_LABEL_LEN];
    unsigned int decimal_address;
    CommandType command : 5;
    unsigned int is_command_line : 1;
    char operands_string[MAX_OPERAND_LEN];
    unsigned int binary_machine_code : 10;
    // NEW: source line number in the original asm file
    unsigned int original_line_number;
} Row;

typedef struct {
    Row *data;
    int size;
    int capacity;
} Table;

Table* create_table();
void free_table(Table *tbl);
void ensure_capacity(Table *tbl);

// UPDATED: pass source line number instead of original line string
void add_row   (Table *tbl, const char *label, CommandType cmd, int is_cmd_line,
                const char *operands, unsigned int binary_code, unsigned int original_line_number);
void edit_row  (Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line,
                const char *operands, unsigned int binary_code, unsigned int original_line_number);
void insert_row(Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line,
                const char *operands, unsigned int binary_code, unsigned int original_line_number);

Row* get_row(Table *tbl, int index);
void reset_addresses(Table *tbl, unsigned int offset);
void print_table(Table *tbl);

#endif //TABLE_H
