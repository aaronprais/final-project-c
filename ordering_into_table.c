#include "ordering_into_table.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "table.h"
#include "labels.h"

void split_matrix_name_and_location(const char *input, char *name, char *rest, size_t max_len) {
    const char *bracket_pos = strchr(input, SQUARE_BRACKET_START_CHAR);

    if (bracket_pos) {
        size_t name_len = (size_t)(bracket_pos - input);
        if (name_len >= max_len) name_len = max_len - ONE; /* prevent overflow */
        strncpy(name, input, name_len);
        name[name_len] = NULL_CHAR;

        strncpy(rest, bracket_pos, max_len - ONE);
        rest[max_len - ONE] = NULL_CHAR;
    } else {
        strncpy(name, input, max_len - ONE);
        name[max_len - ONE] = NULL_CHAR;
        rest[ZERO] = NULL_CHAR;
    }
}

void add_operand(Table *tbl, char *operand, int command, int operand_number, unsigned int src_line_no) {
    if (strcmp(operand, EMPTY_STRING) == 0) {
        if (command==STR)
            add_row(tbl, EMPTY_STRING, command, ZERO, "\0", ZERO, src_line_no);
        else
            add_row(tbl, EMPTY_STRING, command, ZERO, ZERO_STRING, ZERO, src_line_no);
        return;
    }

    /* trim leading/trailing whitespace in-place */
    while (isspace((unsigned char)*operand)) operand++;
    char *end = operand + strlen(operand) - ONE;
    while (end > operand && isspace((unsigned char)*end)) *end-- = NULL_CHAR;
    *(end + ONE) = NULL_CHAR;

    if (is_matrix(operand) != NOT_FOUND) {
        char matrix_name[MAX_OPERAND_LEN];
        char index_pair[MAX_OPERAND_LEN];

        split_matrix_name_and_location(operand, matrix_name, index_pair, MAX_OPERAND_LEN);

        add_row(tbl, EMPTY_STRING, command, ZERO, matrix_name, operand_number, src_line_no);
        add_row(tbl, EMPTY_STRING, command, ZERO, index_pair,  operand_number, src_line_no);
    } else {
        add_row(tbl, EMPTY_STRING, command, ZERO, operand, operand_number, src_line_no);
    }
}

int add_command_to_table(Table *tbl, Labels *lbls, char *label, int command,
                         char *operands_string, int src_line, const char *src_filename) {

    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    char *operand1 = strtok(operands_copy, COMMA_STRING);
    char *operand2 = NULL;
    char *operand3 = NULL;

    if (operand1 != NULL) {
        operand2 = strtok(NULL, COMMA_STRING);
        if (operand2 != NULL) {
            operand3 = strtok(NULL, COMMA_STRING);
            if (operand3 != NULL) {
                print_error(src_filename, src_line, "Too many operands provided");
                return FALSE;
            }
        }
    }

    if (strcmp(label, EMPTY_STRING) != 0)
        add_label_row(lbls, label, tbl->size, CODE, FALSE);

    /* master row for the command line (records original source line number) */
    add_row(tbl, label, command, ONE, operands_string, ZERO, (unsigned int)src_line);

    int expected = command_operands[command];

    if (expected == ZERO && operand1 != NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Too many operands for command \"%s\" (expected 0)",
                 command_names[command]);
        print_error(src_filename, src_line, msg);
        return FALSE;
    }
    if (expected == ONE) {
        if (operand2 != NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too many operands for command \"%s\" (expected 1)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        add_operand(tbl, operand1, command, 1, (unsigned int)src_line);
    }
    else if (expected == TWO) {
        if (operand2 == NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too few operands for command \"%s\" (expected 2)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        if (is_register(operand1) && is_register(operand2)) {
            /* pack two registers into a single operand row */
            add_operand(tbl, operands_string, command, 1, (unsigned int)src_line);
        } else {
            add_operand(tbl, operand1, command, 1, (unsigned int)src_line);
            add_operand(tbl, operand2, command, 2, (unsigned int)src_line);
        }
    }

    return TRUE;
}

int add_data_to_table(Table *tbl, Labels *lbls, char *label, int command,
                      char *operands_string, int src_line, const char *src_filename) {
    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    if (strcmp(label, EMPTY_STRING) != 0)
        add_label_row(lbls, label, tbl->size, DATA, FALSE);

    int first = TRUE;

    if (command == STR) {
        int in_quotes = FALSE;
        int i;
        int quote_count = 0;

        for (i = ZERO; operands_copy[i] != NULL_CHAR; i++) {
            if (operands_copy[i] == '"') {
                in_quotes = !in_quotes;
                quote_count++;
                continue;
            }

            if (in_quotes) {
                char c[2] = {operands_copy[i], NULL_CHAR};
                if (first) {
                    add_row(tbl, label, command, TRUE, c, ZERO, (unsigned int)src_line);
                    first = FALSE;
                } else {
                    add_operand(tbl, c, command, 0, (unsigned int)src_line);
                }
            }
        }

        if (first) {
            print_error(src_filename, src_line,
                        "String directive has no quoted text");
            return FALSE;
        }
        if (in_quotes) {
            print_error(src_filename, src_line,
                        "Unclosed string directive (missing closing quote)");
            return FALSE;
        }
        if (quote_count == 2 && !operands_copy[i - 2]) {
            // special case: "" empty string
            print_error(src_filename, src_line,
                        "Empty string \"\" is not allowed");
            return FALSE;
        }

        /* terminating null byte of string */
        add_operand(tbl, "", command, 0, (unsigned int)src_line);
    }
    else if (command == MAT) {
        int size = is_matrix(operands_string);

        char *operand = strtok(operands_copy, COMMA_STRING);
        int count = ZERO;

        while (count < size) {
            if (operand != NULL) {
                if (first) {
                    char first_str[MAX_OPERAND_LEN];
                    int bracket_count = 0;
                    char *ptr = operand;
                    while (*ptr) {
                        if (*ptr == ']') {
                            bracket_count++;
                            if (bracket_count == 2) {
                                ptr++;
                                strncpy(first_str, ptr, MAX_OPERAND_LEN - 1);
                                first_str[MAX_OPERAND_LEN - 1] = NULL_CHAR;
                                break;
                            }
                        }
                        ptr++;
                    }
                    add_row(tbl, label, command, TRUE, first_str, ZERO, (unsigned int)src_line);
                    first = FALSE;
                } else {
                    add_operand(tbl, operand, command, 0, (unsigned int)src_line);
                }
                operand = strtok(NULL, COMMA_STRING);
            } else {
                if (first) {
                    add_row(tbl, label, command, TRUE, operands_string, ZERO, (unsigned int)src_line);
                    first = FALSE;
                } else {
                    add_operand(tbl, EMPTY_STRING, command, 0, (unsigned int)src_line);
                }
            }
            count++;
        }
        if (operand != NULL) {
            operand = strtok(NULL, COMMA_STRING);
            if (operand != NULL) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Too many values for matrix directive \"%s\"",
                         operands_string);
                print_error(src_filename, src_line, msg);
                return FALSE;
            }
        }
    }
    else {
        char *operand = strtok(operands_copy, COMMA_STRING);

        while (operand != NULL) {
            if (first) {
                add_row(tbl, label, command, TRUE, operand, ZERO, (unsigned int)src_line);
                first = FALSE;
            } else {
                add_operand(tbl, operand, command, 0, (unsigned int)src_line);
            }
            operand = strtok(NULL, COMMA_STRING);
        }
    }

    return TRUE;
}

int process_file_to_table_and_labels(Table *tbl, Labels *lbls, FILE *file, const char *src_filename) {
    char line[MAX_LINE_LENGTH];
    int error = FALSE;
    int src_line = 0;

    rewind(file);

    while (fgets(line, sizeof(line), file)) {
        src_line++;

        char *word = strtok(line, SPLITTING_DELIM);

        if (word != NULL) {
            if (strcmp(word, ENTRY) == 0) {
                char *rest = strtok(NULL, NEW_LINE_STRING);
                add_label_row(lbls, rest, ZERO, UNKNOWN, TRUE);
            }
            else if (strcmp(word, EXTERN) == 0) {
                char *rest = strtok(NULL, NEW_LINE_STRING);
                add_label_row(lbls, rest, ZERO, EXT, FALSE);
            }
            else {
                int command = NOT_FOUND;
                char label[MAX_LABEL_LEN] = EMPTY_STRING;
                char operands_string[MAX_OPERAND_LEN] = EMPTY_STRING;

                if (is_label(word)) {
                    strcpy(label, word);
                    word = strtok(NULL, SPLITTING_DELIM);
                }

                command = find_command(word, label);

                if (command == NOT_FOUND) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Command \"%s\" not recognised", word ? word : "(null)");
                    print_error(src_filename, src_line, msg);
                    error = TRUE;
                }
                else {
                    char *rest = strtok(NULL, NEW_LINE_STRING);
                    if (rest != NULL) {
                        strcpy(operands_string, rest);
                    }

                    if (command < NUMBER_OF_COMMANDS) {
                        if (!add_command_to_table(tbl, lbls, label, command, operands_string,
                                                  src_line, src_filename)) {
                            error = TRUE;
                        }
                    } else {
                        if (!add_data_to_table(tbl, lbls, label, command, operands_string,
                                               src_line, src_filename)) {
                            error = TRUE;
                        }
                    }
                }
            }
        }
    }

    return error;
}
