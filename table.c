#include "table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------- internal helper funcs --------- */

/* safer copy of str to buffer (avoid overflow kinda) */
static void safe_copy(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src) src = EMPTY_STRING;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = NULL_CHAR;
}

/* init row with default values (basicaly reset) */
static void init_row(Row *r) {
    if (!r) return;
    r->label[0] = NULL_CHAR;
    r->decimal_address = 0;
    r->command = 0;
    r->is_command_line = 0;
    r->operands_string[0] = NULL_CHAR;
    r->binary_machine_code = 0;
    r->original_line_number = 0;
}

/* print number as 10-bit binary (simple ver) */
static void print_10bit_binary(unsigned int v) {
    unsigned int mask = 1u << 9; // msb of 10 bits
    int i;
for (i = 0; i < 10; ++i) {
        putchar((v & mask) ? '1' : '0');
        mask >>= 1;
    }
}

/* --------- API funcs --------- */

/* create a table with some capacity */
Table* create_table() {
    Table *tbl = (Table*)malloc(sizeof(Table));
    if (!tbl) return NULL;

    tbl->size = 0;
    tbl->capacity = 16; // start with 16 rows
    tbl->data = (Row*)malloc((size_t)tbl->capacity * sizeof(Row));
    if (!tbl->data) {
        free(tbl);
        return NULL;
    }

    int i;
for (i = 0; i < tbl->capacity; ++i) init_row(&tbl->data[i]);
    return tbl;
}

/* free the table mem (dont forget to call) */
void free_table(Table *tbl) {
    if (!tbl) return;
    free(tbl->data);
    free(tbl);
}

/* make sure there is space for new rows */
void ensure_capacity(Table *tbl) {
    if (!tbl) return;
    if (tbl->size < tbl->capacity) return;

    int new_cap = tbl->capacity * 2;
    if (new_cap < 1) new_cap = 1;

    Row *new_data = (Row*)realloc(tbl->data, (size_t)new_cap * sizeof(Row));
    if (!new_data) {
        fprintf(stderr, "ensure_capacity: realloc fail (req cap=%d)\n", new_cap);
        return;
    }

    int i;
for (i = tbl->capacity; i < new_cap; ++i) init_row(&new_data[i]);

    tbl->data = new_data;
    tbl->capacity = new_cap;
}

/* add new row to the end */
void add_row(Table *tbl, const char *label, CommandType cmd, int is_cmd_line,
             const char *operands, unsigned int binary_code, unsigned int original_line_number) {
    if (!tbl) return;
    ensure_capacity(tbl);
    if (tbl->size >= tbl->capacity) return; // realloc failed maybe

    Row *r = &tbl->data[tbl->size];
    init_row(r);

    safe_copy(r->label, label, sizeof(r->label));
    r->command = cmd;
    r->is_command_line = (is_cmd_line != 0);
    safe_copy(r->operands_string, operands, sizeof(r->operands_string));
    r->binary_machine_code = (binary_code & 0x3FFu); // only 10 bits
    r->original_line_number = original_line_number;
    r->decimal_address = 0; // will reset later

    tbl->size += 1;
}

/* edit a row at given index (keep addr same) */
void edit_row(Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line,
              const char *operands, unsigned int binary_code, unsigned int original_line_number) {
    if (!tbl) return;
    if (index < 0 || index >= tbl->size) return;

    Row *r = &tbl->data[index];

    safe_copy(r->label, label, sizeof(r->label));
    r->command = cmd;
    r->is_command_line = (is_cmd_line != 0);
    safe_copy(r->operands_string, operands, sizeof(r->operands_string));
    r->binary_machine_code = (binary_code & 0x3FFu);
    r->original_line_number = original_line_number;
    // note: decimal addr not changed here
}

/* insert new row at index (shift right) */
void insert_row(Table *tbl, int index, const char *label, CommandType cmd, int is_cmd_line,
                const char *operands, unsigned int binary_code, unsigned int original_line_number) {
    if (!tbl) return;

    if (index < 0) index = 0;
    if (index > tbl->size) index = tbl->size;

    ensure_capacity(tbl);
    if (tbl->size >= tbl->capacity) return;

    // move old rows to the right
    int i;
for (i = tbl->size; i > index; --i) {
        tbl->data[i] = tbl->data[i - 1];
    }
    init_row(&tbl->data[index]);

    Row *r = &tbl->data[index];
    safe_copy(r->label, label, sizeof(r->label));
    r->command = cmd;
    r->is_command_line = (is_cmd_line != 0);
    safe_copy(r->operands_string, operands, sizeof(r->operands_string));
    r->binary_machine_code = (binary_code & 0x3FFu);
    r->original_line_number = original_line_number;
    r->decimal_address = 0;

    tbl->size += 1;
}

/* get row pointer (or null if invalid index) */
Row* get_row(Table *tbl, int index) {
    if (!tbl) return NULL;
    if (index < 0 || index >= tbl->size) return NULL;
    return &tbl->data[index];
}

/* reset decimal addresses starting from offset */
void reset_addresses(Table *tbl, unsigned int offset) {
    if (!tbl) return;

    unsigned int addr = offset;
    int i;
for (i = 0; i < tbl->size; ++i) {
        Row *r = &tbl->data[i];
        r->decimal_address = addr;
        addr++;
    }
}

/* print ascii table of all rows */
void print_table(Table *tbl) {
    if (!tbl) return;

    printf("\n%-5s | %-20s | %-6s | %-3s | %-30s | %-12s | %-6s\n",
           "ADDR", "LABEL", "CMD", "CL", "OPERANDS", "BIN(10b)", "SRC#");
    printf("-----+-" "--------------------" "-+-" "------" "-+-" "---" "-+-"
           "------------------------------" "-+-" "------------" "-+-" "------" "\n");

    int i;
for (i = 0; i < tbl->size; ++i) {
        Row *r = &tbl->data[i];

        printf("%-5u | %-20s | %-6u | %-3u | %-30s | ",
               r->decimal_address,
               r->label,
               (unsigned)r->command,
               (unsigned)r->is_command_line,
               r->operands_string);

        print_10bit_binary(r->binary_machine_code);
        printf(" | %-6u\n", r->original_line_number);
    }
}
