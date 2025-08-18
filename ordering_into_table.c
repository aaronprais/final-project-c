#include "ordering_into_table.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "table.h"
#include "labels.h"

/* assumes find_label_by_name(...) is declared in labels.h:
   Label* find_label_by_name(const Labels *lbls, const char *name); */

/* Hard cap on number of rows (program lines) allowed in the table. */
#ifndef MAX_TABLE_ROWS
#define MAX_TABLE_ROWS 255
#endif

/*
 * check_table_overflow
 * --------------------
 * After any add_row, we validate we didn't blow past MAX_TABLE_ROWS.
 * Emits a nice error with filename + source line.
 * returns TRUE if ok, FALSE if overflow.
 */
static int check_table_overflow(Table *tbl, const char *src_filename, int src_line) {
    if (tbl && tbl->size > MAX_TABLE_ROWS) {
        print_error(src_filename, src_line, "Program exceeds maximum of 255 lines");
        return FALSE;
    }
    return TRUE;
}

/*
 * split_matrix_name_and_location
 * ------------------------------
 * Given something like "M1[2][3]" it seperates:
 *   name -> "M1"
 *   rest -> "[2][3]"
 * We try to be careful not to overflow fixed-size buffers.
 */
void split_matrix_name_and_location(const char *input, char *name, char *rest, size_t max_len) {
    const char *bracket_pos = strchr(input, SQUARE_BRACKET_START_CHAR);

    if (bracket_pos) {
        size_t name_len = (size_t)(bracket_pos - input);
        if (name_len >= max_len) name_len = max_len - 1; /* prevent overflow */
        strncpy(name, input, name_len);
        name[name_len] = NULL_CHAR;

        strncpy(rest, bracket_pos, max_len - 1);
        rest[max_len - 1] = NULL_CHAR;
    } else {
        strncpy(name, input, max_len - 1);
        name[max_len - 1] = NULL_CHAR;
        rest[0] = NULL_CHAR;
    }
}

/*
 * add_operand
 * -----------
 * Normalizes an operand (trims whitespace), handles matrix/register packing etc,
 * and appends the corresponding row(s) into the table.
 * Note: returns TRUE on success, FALSE on any error (keeps behavior same).
 */
int add_operand(Table *tbl, char *operand, int command, int operand_number,
                unsigned int src_line_no, const char *src_filename) {
    if (strcmp(operand, EMPTY_STRING) == 0) {
        /* empty operand is valid for some directives (e.g. .string terminator) */
        if (command == STR) {
            add_row(tbl, EMPTY_STRING, command, 0, "\0", 0, src_line_no);
        } else {
            add_row(tbl, EMPTY_STRING, command, 0, "0", 0, src_line_no);
        }
        return check_table_overflow(tbl, src_filename, (int)src_line_no);
    }

    /* trim leading/trailing whitespace in-place (classic c-string trick) */
    while (isspace((unsigned char)*operand)) operand++;
    char *end = operand + strlen(operand) - 1;
    while (end > operand && isspace((unsigned char)*end)) *end-- = NULL_CHAR;
    *(end + 1) = NULL_CHAR;

    /* matrix operand is split into 2 rows: name and index part(s) */
    if (is_matrix(operand) != NOT_FOUND) {
        char matrix_name[MAX_OPERAND_LEN];
        char index_pair[MAX_OPERAND_LEN];

        split_matrix_name_and_location(operand, matrix_name, index_pair, MAX_OPERAND_LEN);

        add_row(tbl, EMPTY_STRING, command, 0, matrix_name, operand_number, src_line_no);
        if (!check_table_overflow(tbl, src_filename, (int)src_line_no)) return FALSE;

        add_row(tbl, EMPTY_STRING, command, 0, index_pair, operand_number, src_line_no);
        if (!check_table_overflow(tbl, src_filename, (int)src_line_no)) return FALSE;
    } else {
        /* normal (non-matrix) operand */
        add_row(tbl, EMPTY_STRING, command, 0, operand, operand_number, src_line_no);
        if (!check_table_overflow(tbl, src_filename, (int)src_line_no)) return FALSE;
    }

    return TRUE;
}

/*
 * add_command_to_table
 * --------------------
 * Handles instruction-like lines (not data directives),
 * validates the number of operands, and records packed registers when posible.
 */
int add_command_to_table(Table *tbl, Labels *lbls, char *label, int command,
                         char *operands_string, int src_line, const char *src_filename) {

    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    /* quick comma count to catch extra/missing commas later */
    int comma_count = 0;
    const char *p;
    for (p = operands_copy; *p; p++) {
        if (*p == ',') comma_count++;
    }

    /* tokenize into up to 2 operands (fits ISA specs) */
    char *operand1 = strtok(operands_copy, COMMA_STRING);
    char *operand2 = NULL;
    if (operand1 != NULL) {
        operand2 = strtok(NULL, COMMA_STRING);
    }

    if (strcmp(label, EMPTY_STRING) != 0) {
        /* Labels can be reused only in very specific .entry scenarious (enforced below).
           NOTE: we keep the original error message text (with its typos) for compat. */
        Label *existing = find_label_by_name(lbls, label);
        if (existing && !existing->is_entry) {
            char msg[128];
            /* exact phrasing requested: “lable alrady exists” */
            snprintf(msg, sizeof(msg), "lable alrady exists: \"%s\"", label);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        /* record code label location (IC) */
        if (!add_label_row(lbls, label, tbl->size, CODE, FALSE, src_line, src_filename)) {
            return FALSE;
        }
    }

    /* master row for the command line (records original source line number) */
    add_row(tbl, label, command, 1, operands_string, 0, (unsigned int)src_line);
    if (!check_table_overflow(tbl, src_filename, src_line)) return FALSE;

    int expected = command_operands[command];

    /* validate arity — classic fence-post checks */
    if (expected == 0 && operand1 != NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Too many operands for command \"%s\" (expected 0)",
                 command_names[command]);
        print_error(src_filename, src_line, msg);
        return FALSE;
    }
    if (expected == 1) {
        if (operand2 != NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too many operands for command \"%s\" (expected 1)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        if (operand1 == NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too few operands for command \"%s\" (expected 1)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        if (!add_operand(tbl, operand1, command, 1, (unsigned int)src_line, src_filename))
            return FALSE;
    }
    else if (expected == 2) {
        if (operand2 == NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too few operands for command \"%s\" (expected 2)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        if (operand1 == NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Too few operands for command \"%s\" (expected 2)",
                     command_names[command]);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }

        /* micro-optimization: 2 registers can be packed into 1 row */
        if (is_register(operand1) != FALSE && is_register(operand2) != FALSE) {
            if (!add_operand(tbl, operands_string, command, 1, (unsigned int)src_line, src_filename))
                return FALSE;
        } else {
            if (!add_operand(tbl, operand1, command, 1, (unsigned int)src_line, src_filename))
                return FALSE;
            if (!add_operand(tbl, operand2, command, 2, (unsigned int)src_line, src_filename))
                return FALSE;
        }
    }

    /* comma policy: catch extra or missing 1s (pretty common student bug btw) */
    if ((expected > 0 && comma_count > expected - 1) || (expected == 0 && comma_count > 0)) {
        print_error(src_filename, src_line, "Unexpected extra comma");
        return FALSE;
    }
    if (expected > 1 && comma_count < expected - 1) {
        print_error(src_filename, src_line, "Missing comma");
        return FALSE;
    }

    return TRUE;
}

/*
 * add_data_to_table
 * -----------------
 * Handles data directives like .data / .string / .mat etc.
 * Careful with quotes and matrix sizes; we mimic the assembler rules exactly.
 */
int add_data_to_table(Table *tbl, Labels *lbls, char *label, int command,
                      char *operands_string, int src_line, const char *src_filename) {
    char operands_copy[MAX_OPERAND_LEN];
    strncpy(operands_copy, operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    if (strcmp(label, EMPTY_STRING) != 0) {
        /* Same reuse rule as in commands. Keep original message strings (typos and all). */
        Label *existing = find_label_by_name(lbls, label);
        if (existing && !existing->is_entry) {
            char msg[128];
            /* exact phrasing requested */
            snprintf(msg, sizeof(msg), "lable alrady exists: \"%s\"", label);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
        if (!add_label_row(lbls, label, tbl->size, DATA, FALSE, src_line, src_filename)) {
            return FALSE;
        }
    }

    int first = TRUE;

    if (command == STR) {
        /* parse chars between quotes as separate bytes */
        int in_quotes = FALSE;
        int after_quotes = FALSE;
        int i;
        int quote_count = 0;

        for (i = 0; operands_copy[i] != NULL_CHAR; i++) {

            if (after_quotes && !isspace(operands_copy[i])) {
                print_error(src_filename, src_line, "String after quotes has text");
            }

            if (operands_copy[i] == '"') {
                if (in_quotes) {
                    after_quotes = TRUE;
                    continue;
                }
                in_quotes = TRUE;
                quote_count++;
                continue;
            }

            if (in_quotes) {
                char c[2] = {operands_copy[i], NULL_CHAR};
                if (first) {
                    add_row(tbl, label, command, TRUE, c, 0, (unsigned int)src_line);
                    if (!check_table_overflow(tbl, src_filename, src_line)) return FALSE;
                    first = FALSE;
                } else {
                    if (!add_operand(tbl, c, command, 0, (unsigned int)src_line, src_filename))
                        return FALSE;
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
            /* special case: "" empty string (not allowed in spec here) */
            print_error(src_filename, src_line,
                        "Empty string \"\" is not allowed");
            return FALSE;
        }

        /* terminating null byte of string (end-of-text marker) */
        if (!add_operand(tbl, "", command, 0, (unsigned int)src_line, src_filename))
            return FALSE;
    }
    else if (command == MAT) {
        /* MAT expects a fixed count of values based on the matrix size specifier */
        int size = is_matrix(operands_string);

        char *operand = strtok(operands_copy, COMMA_STRING);
        int count = 0;

        while (count < size) {
            if (operand != NULL) {
                if (first) {
                    /* For the first item, we must strip the "[..][..]" part and keep the value.
                       This is a bit fiddly but works fine — dont change pls :) */
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
                    add_row(tbl, label, command, TRUE, first_str, 0, (unsigned int)src_line);
                    if (!check_table_overflow(tbl, src_filename, src_line)) return FALSE;
                    first = FALSE;
                } else {
                    if (!add_operand(tbl, operand, command, 0, (unsigned int)src_line, src_filename))
                        return FALSE;
                }
                operand = strtok(NULL, COMMA_STRING);
            } else {
                /* If fewer values than needed, pad with EMPTY_STRING (assembler semantics) */
                if (first) {
                    add_row(tbl, label, command, TRUE, operands_string, 0, (unsigned int)src_line);
                    if (!check_table_overflow(tbl, src_filename, src_line)) return FALSE;
                    first = FALSE;
                } else {
                    if (!add_operand(tbl, EMPTY_STRING, command, 0, (unsigned int)src_line, src_filename))
                        return FALSE;
                }
            }
            count++;
        }
        /* If there are *more* values than size, thats an error */
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
        /* .data-like list of numbers/operands separated by commas */
        char *operand = strtok(operands_copy, COMMA_STRING);

        while (operand != NULL) {
            if (first) {
                add_row(tbl, label, command, TRUE, operand, 0, (unsigned int)src_line);
                if (!check_table_overflow(tbl, src_filename, src_line)) return FALSE;
                first = FALSE;
            } else {
                if (!add_operand(tbl, operand, command, 0, (unsigned int)src_line, src_filename))
                    return FALSE;
            }
            operand = strtok(NULL, COMMA_STRING);
        }
    }

    return TRUE;
}

/*
 * process_file_to_table_and_labels
 * --------------------------------
 * Top-level driver: loops over input lines, splits optional label, detects directive
 * vs command, and dispatches to the proper helper. Keeps the original error text.
 */
int process_file_to_table_and_labels(Table *tbl, Labels *lbls, FILE *file, const char *src_filename) {
    char line[MAX_LINE_LENGTH];
    int error = FALSE;
    int src_line = 0;

    rewind(file);

    while (fgets(line, sizeof(line), file)) {
        src_line++;

        /* -------- Remove trailing newline, if any -------- */
        {
            char *nl = strchr(line, '\n');
            if (nl) *nl = NULL_CHAR;
        }

        /* skip empty/comment-only lines early for speed (tiny micro-optim) */
        if (line[0] == NULL_CHAR || line[0] == NEWLINE_CHAR || line[0] == COMMENT_CHAR) {
            /* Skip empty lines */
            continue;
        }
        if (line[0] == SEMI_COLON_CHAR) {
            /* colon at start means there was a ':' with no label chars before it */
            print_error(src_filename, src_line, "Empty label");
            error = TRUE;
        }

        /* -------- Split optional label by first ':' -------- */
        char label[MAX_LABEL_LEN] = EMPTY_STRING;
        char *after = line;
        char *colon = strchr(line, SEMI_COLON_CHAR); /* ':' */

        if (colon) {
            *colon = NULL_CHAR;           /* terminate label part */
            after  = colon + 1;           /* rest of the line */

            /* Trim the label part (leading/trailing spaces) */
            {
                char *start = line;
                while (*start && isspace((unsigned char)*start)) start++;
                char *end = start + (int)strlen(start);
                while (end > start && isspace((unsigned char)*(end - 1))) end--;
                *end = NULL_CHAR;

                if (strlen(start) >= MAX_LABEL_LEN) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Label too long: \"%s\"", start);
                    print_error(src_filename, src_line, msg);
                    error = TRUE;
                }

                /* store label (already without ':') */
                if (*start) {
                    strncpy(label, start, MAX_LABEL_LEN - 1);
                    label[MAX_LABEL_LEN - 1] = NULL_CHAR;
                }
            }
        }

        /* -------- Tokenize the rest of the line for directive/command -------- */
        char *word = strtok(after, SPLITTING_DELIM);

        if (word != NULL) {
            if (strcmp(word, ENTRY) == 0) {
                /* .entry <label> — mark a symbol as entry point */
                char *rest = strtok(NULL, NEW_LINE_STRING); /* label name (no ':') */
                if (!rest) {
                    print_error(src_filename, src_line, ".entry requires a label");
                    error = TRUE;
                }
                else {
                    Label *existing = find_label_by_name(lbls, rest);

                    if (existing && (existing->is_entry || count_label_by_name(lbls, rest) > 1)) {
                        char msg[128];
                        /* exact phrasing requested */
                        snprintf(msg, sizeof(msg), "lable alrady exists: \"%s\"", rest ? rest : "(null)");
                        print_error(src_filename, src_line, msg);
                        error = TRUE;
                    } else {
                        if (!add_label_row(lbls, rest, 0, UNKNOWN, TRUE, src_line, src_filename)) {
                            error = TRUE;  /* fixed: was FALSE */
                        }
                    }
                }
            }
            else if (strcmp(word, EXTERN) == 0) {
                /* .extern <label> — declare an external symbol */
                char *rest = strtok(NULL, NEW_LINE_STRING); /* label name (no ':') */

                if (!rest) {
                    print_error(src_filename, src_line, ".extern requires a label");
                    error = TRUE;
                }
                else {
                    Label *existing = find_label_by_name(lbls, rest);
                    if (existing) {
                        char msg[128];
                        /* exact phrasing requested */
                        snprintf(msg, sizeof(msg), "lable alrady exists: \"%s\"", rest ? rest : "(null)");
                        print_error(src_filename, src_line, msg);
                        error = TRUE;
                    } else {
                        if (!add_label_row(lbls, rest, 0, EXT, FALSE, src_line, src_filename)) {
                            error = TRUE;  /* fixed: was FALSE */
                        }
                    }
                }
            }
            else {
                /* otherwise it's a regular command or a data directive */
                int command = NOT_FOUND;
                char operands_string[MAX_OPERAND_LEN] = EMPTY_STRING;

                /* Find command using the (possibly empty) label detected before ':' */
                command = find_command(word, label);

                if (command == NOT_FOUND) {
                    /* Unrecognized mnemonic or directive */
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Command \"%s\" not recognised", word ? word : "(null)");
                    print_error(src_filename, src_line, msg);
                    error = TRUE;
                }
                else {
                    /* grab rest of the line as raw operands string (could be empty) */
                    char *rest = strtok(NULL, NEW_LINE_STRING);
                    if (rest != NULL) {
                        strcpy(operands_string, rest);
                    }

                    /* command range: < NUMBER_OF_COMMANDS means “real instruction” */
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

        /* Early-out if we already overflowed to avoid cascading errors (good UX) */
        if (tbl->size > MAX_TABLE_ROWS) {
            /* Specific overflow error should have been printed at point of failure. */
            break;
        }
    }

    return error;
}
