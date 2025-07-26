#include <stdio.h>
#include "binary_table_parsing.h"
#include <string.h>
#include "table.h"
#include "labels.h"
#include "util.h"


int command_line_binary(Row *row) {
    // ensure only 4 bits from command
    int value = (row->command & 0xF) << 6; // shift to the top 4 bits of a 10-bit field

    // assign to row
    row->binary_machine_code = value;

    return TRUE;
}

int data_line_binary(Row *row) {
    if (row->command == STR) {
        // keep only the lowest 10 bits
        int value = row->operands_string[0];
        int encoded = value & 0x3FF;   // 0x3FF = 10 bits of 1
        row->binary_machine_code = encoded;
        return TRUE;
    }
    if (row->command == DAT) {
        double number;
        if (is_number(row->operands_string, &number)) {
            row->binary_machine_code = number;
            return TRUE;
        }
    }
    if (row->command == MAT) {
        if (row->is_command_line) {
            // Find the second closing bracket ']'
            const char *p = row->operands_string;
            const char *after_second = NULL;

            // find first ']'
            p = strchr(p, ']');
            if (p) {
                p++; // move past first bracket
                // find second ']'
                p = strchr(p, ']');
                if (p) {
                    after_second = p + 1; // pointer after second bracket
                }
            }

            if (after_second) {
                double number;
                if (is_number(after_second, &number)) {
                    row->binary_machine_code = ((int)number) & 0x3FF;
                    return TRUE;
                }
            }
        }
        else {
            double number;
            if (is_number(row->operands_string, &number)) {
                row->binary_machine_code = number;
                return TRUE;
            }
        }
    }

    return FALSE;
}

int parse_table_to_binary(Table *table, Labels *labels) {
    int index = ZERO;

    for (index = ZERO; index < table->size; index++) {
        Row *row = get_row(table, index);

        if (row->is_command_line) {
            if (row->command < NUMBER_OF_COMMANDS) {
                command_line_binary(row);
            }
            else if (row->command >= NUMBER_OF_COMMANDS) {
                data_line_binary(row);
            }

        }
        else {
            if (row->command < NUMBER_OF_COMMANDS) {

            }
            else if (row->command >= NUMBER_OF_COMMANDS) {
                data_line_binary(row);
            }
        }
    }

    return TRUE;
}
