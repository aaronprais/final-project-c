#include "ordering_into_table.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "table.h"
#include "labels.h"

void split_matrix_name_and_location(const char *input, char *name, char *rest, size_t max_len) {
    // Find the first '[' in the input
    const char *bracket_pos = strchr(input, SQUARE_BRACKET_START_CHAR);

    if (bracket_pos) {
        // Copy everything before '[' into name
        size_t name_len = bracket_pos - input;
        if (name_len >= max_len) name_len = max_len - ONE; // prevent overflow
        strncpy(name, input, name_len);
        name[name_len] = NULL_CHAR;

        // Copy from '[' to end into rest
        strncpy(rest, bracket_pos, max_len - ONE);
        rest[max_len - ONE] = NULL_CHAR;
    } else {
        // No '[' found: whole input is name, rest is empty
        strncpy(name, input, max_len - ONE);
        name[max_len - ONE] = NULL_CHAR;
        rest[ZERO] = NULL_CHAR;
    }
}

void add_operand(Table *tbl, char *operand, int command) {

    if (strcmp(operand, EMPTY_STRING) == 0) {
        add_row(tbl, EMPTY_STRING,ZERO,ZERO, ZERO_STRING,ZERO);
        return;
    }

    // Trim leading/trailing spaces
    while (isspace(*operand)) operand++;
    char *end = operand + strlen(operand) - ONE;
    while (end > operand && isspace(*end)) *end-- = NULL_CHAR;
    *(end + ONE) = NULL_CHAR;

    if (is_matrix(operand) != NOT_FOUND) {
        char matrix_name[MAX_OPERAND_LEN];
        char index_pair[MAX_OPERAND_LEN];

        split_matrix_name_and_location(operand, matrix_name, index_pair, MAX_OPERAND_LEN);

        // First row: matrix name
        add_row(tbl, EMPTY_STRING,command,ZERO, matrix_name,ZERO);

        // Second row: both registers in one string like "[r2],[r3]"
        add_row(tbl, EMPTY_STRING,command,ZERO, index_pair,ZERO);
    } else {
        // Just one operand
        add_row(tbl, EMPTY_STRING,command,ZERO, operand,ZERO);
    }
}

void add_command_to_table(Table *tbl, Labels *lbls, char *label, int command, char *operands_string){

    // Preserve original operands_string before strtok modifies it
    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    // Tokenize the operands string into up to 3 parts
    char *operand1 = strtok(operands_copy,  COMMA_STRING);
    char *operand2 = NULL;
    char *operand3 = NULL;

    if (operand1 != NULL) {
        operand2 = strtok(NULL,  COMMA_STRING);
        if (operand2 != NULL) {
            operand3 = strtok(NULL,  COMMA_STRING);
            if (operand3 != NULL) {
                printf("Too Many Opperands");
                return;
            }
        }
    }

    if (strcmp(label, EMPTY_STRING) != 0)
        add_label_row(lbls, label, tbl->size, CODE, FALSE);

    add_row(tbl, label, command, ONE, operands_string,ZERO); // Assuming add_row is defined

    int expected = command_operands[command];

    if (expected == ZERO && operand1 != NULL) {
        printf("Too many operands, expected 0");
        return;
    }
    if (expected == ONE) {
        if (operand2 != NULL) {
            printf("Error: too many operands. Expected 1, got more.\n");
            return;
        }
        // If one operand command
        add_operand(tbl, operand1, command); // even if NULL, your add_operand should handle it
    }
    else if (expected == TWO) {
        if (is_register(operand1) && is_register(operand2)) {
            add_operand(tbl, operands_string, command);
        }
        else {
            add_operand(tbl, operand1, command);
            add_operand(tbl, operand2, command);
        }
    }

}

void add_data_to_table(Table *tbl, Labels *lbls, char *label, int command, char *operands_string){

    // Preserve original operands_string before strtok modifies it
    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    if (strcmp(label, EMPTY_STRING) != 0)
        add_label_row(lbls, label, tbl->size, DATA, FALSE);

    int first = TRUE;

    if (command == STR) {
        int in_quotes = FALSE;
        int i;

        for (i = ZERO; operands_copy[i] != NULL_CHAR; i++) {
            if (operands_copy[i] == '"') {
                // toggle inside/outside quotes
                in_quotes = !in_quotes;
                continue;
            }

            if (in_quotes) {
                char c[2] = {operands_copy[i], NULL_CHAR};
                // process character inside quotes
                if (first) {
                    add_row(tbl, label, command, TRUE, c, ZERO);
                    first = FALSE;
                }// Assuming add_row is defined
                else {
                    add_operand(tbl, c, command);
                }
            }
        }
        add_operand(tbl, "", command);
    }
    else if (command == MAT) {
        int size = is_matrix(operands_string);

        // Option 1: If you want to process exactly 'size' operands from the string
        char *operand = strtok(operands_copy,  COMMA_STRING);
        int count = ZERO;

        while (count < size) {
            if (operand != NULL) {
                if (first) {
                    add_row(tbl, label, command, TRUE, operand, ZERO);
                    first = FALSE;
                }
                else {
                    add_operand(tbl, operand, command);
                }
                operand = strtok(NULL,  COMMA_STRING);  // Get next operand
            }
            else {
                // No more operands in string, but we still need to fill 'size' slots
                if (first) {
                    add_row(tbl, label, command, TRUE, operands_string, ZERO);
                    first = FALSE;
                }
                else {
                    add_operand(tbl, EMPTY_STRING, command);
                }
            }
            count++;
        }
        if (operand != NULL) {
            operand = strtok(NULL,  COMMA_STRING);
            if (operand != NULL) {
                printf("Too many values into table size");
            }
        }
    }
    else{
        // Tokenize the operands string
        char *operand = strtok(operands_copy, COMMA_STRING);

        while (operand != NULL) {

            if (first) {
                add_row(tbl, label, command,TRUE, operand, ZERO);
                first = FALSE;
            }
            else {
                add_operand(tbl, operand, command);
            }
            operand = strtok(NULL, COMMA_STRING);
        }
    }
}

// Example signature: file pointer and array of labels with count
void process_file_to_table_and_labels(Table *tbl, Labels *lbls, FILE *file) {
    char line[MAX_LINE_LENGTH];

    // Rewind file to start reading from beginning
    rewind(file);

    // Read file line by line
    while (fgets(line, sizeof(line), file)) {
        // Pointer to tokenize line into words
        char *word = strtok(line, SPLITTING_DELIM);

        if (word != NULL) {
            if (strcmp(word, ENTRY) == 0) {
                char *rest = strtok(NULL,  NEW_LINE_STRING);
                add_label_row(lbls, rest, ZERO, UNKNOWN, TRUE);
            }
            else if (strcmp(word, EXTERN) == 0) {
                char *rest = strtok(NULL,  NEW_LINE_STRING);
                add_label_row(lbls, rest, ZERO, EXT, FALSE);
            }
            else {
                int command = NOT_FOUND;
                char label[MAX_LABEL_LEN] = EMPTY_STRING;
                char operands_string[MAX_OPERAND_LEN] = EMPTY_STRING;

                if (is_label(word)){
                    strcpy(label, word);
                    word = strtok(NULL,  SPLITTING_DELIM);
                }

                command = find_command(word, label);

                if (command == NOT_FOUND) {
                    printf("Command not recognised.\n");
                }
                else {
                    if (command < NUMBER_OF_COMMANDS) {
                        char *rest = strtok(NULL,  NEW_LINE_STRING);
                        if (rest != NULL) {
                            strcpy(operands_string, rest);
                        }
                        add_command_to_table(tbl, lbls, label, command, operands_string);
                    }
                    else {
                        char *rest = strtok(NULL,  NEW_LINE_STRING);
                        if (rest != NULL) {
                            strcpy(operands_string, rest);
                        }
                        add_data_to_table(tbl, lbls, label, command, operands_string);
                    }
                }
            }
        }
    }
}