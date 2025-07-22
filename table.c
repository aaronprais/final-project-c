#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "util.h"

// Initialize a new table
Table* create_table() {
    Table *tbl = malloc(sizeof(Table));
    if (!tbl) {
        perror("Failed to allocate table");
        exit(EXIT_FAILURE);
    }
    tbl->data = NULL;
    tbl->size = ZERO;
    tbl->capacity = ZERO;
    return tbl;
}

// Free table memory
void free_table(Table *tbl) {
    if (tbl) {
        free(tbl->data);
        free(tbl);
    }
}

// Ensure table has space
void ensure_capacity(Table *tbl) {
    if (tbl->size >= tbl->capacity) {
        tbl->capacity = tbl->capacity == ZERO ? FOUR : tbl->capacity * TWO;
        row *new_data = realloc(tbl->data, tbl->capacity * sizeof(row));
        if (!new_data) {
            perror("Failed to realloc table");
            exit(EXIT_FAILURE);
        }
        tbl->data = new_data;
    }
}

// Add a new row
void add_row(Table *tbl, const char *label, CommandType cmd, int is_cmd_line, const char *operands, unsigned int binary_code) {
    ensure_capacity(tbl);
    strncpy(tbl->data[tbl->size].label, label, MAX_LABEL_LEN);
    tbl->data[tbl->size].label[LABEL_NULL_CHAR_LOCATION] = NULL_CHAR;

    tbl->data[tbl->size].decimal_address = ZERO;
    tbl->data[tbl->size].command = cmd;
    tbl->data[tbl->size].is_command_line = is_cmd_line;

    strncpy(tbl->data[tbl->size].operands_string, operands, MAX_OPERAND_LEN);
    tbl->data[tbl->size].operands_string[OPERAND_NULL_CHAR_LOCATION] = NULL_CHAR;

    tbl->data[tbl->size].binary_machine_code = binary_code;

    tbl->size++;
}

// Edit an existing row by index
void edit_row_binary_code(Table *tbl, int index, unsigned int binary_code) {
    if (index < ZERO || index >= tbl->size) {
        printf("Invalid row index %d\n", index);
        return;
    }
    tbl->data[index].binary_machine_code = binary_code;
}

// Get pointer to a row by index
row* get_row(Table *tbl, int index) {
    if (index < ZERO || index >= tbl->size) {
        printf("Invalid index %d\n", index);
        return NULL;
    }
    return &tbl->data[index];
}

// Adds 'offset' to decimal_address of every row in the table
void reset_addresses(Table *tbl, unsigned int offset) {
    int i;
    for (i = ZERO; i < tbl->size; i++) {
        tbl->data[i].decimal_address = offset + i;
    }
}

void print_table(Table *tbl) {
    printf("Row | Label           | Addr | Cmd | Operands                    | Binary Code\n");
    printf("----+------------------+------+-----+-----------------------------+-------------\n");
    int i;
    for (i = 0; i < tbl->size; i++) {
        printf("%-3d | %-15s | %4u | ",
               i,
               tbl->data[i].label,
               tbl->data[i].decimal_address);

        if (tbl->data[i].is_command_line) {
            printf("%3u", tbl->data[i].command);
        } else {
            printf(" --");
        }

        printf(" | %-27s | %010u\n",
               tbl->data[i].operands_string,
               tbl->data[i].binary_machine_code);
    }
}