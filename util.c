#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "util.h"
#include "labels.h"

/* this func trys to find a comand by name or label */
int find_command(char *word, char *label) {
    int i;
for (i = 0; i < NUMBER_OF_COMMANDS; i++) {
        if (strcmp(word, command_names[i]) == 0) {
            return i; // found normal command
        }
        if (i < NUMBER_OF_DATA_TYPES &&
            strcmp(label, EMPTY_STRING) != 0 &&
            strcmp(word, command_names[i + NUMBER_OF_COMMANDS]) == 0) {
            return i + NUMBER_OF_COMMANDS; // found data type command
        }
    }
    return NOT_FOUND; // nothing was found
}

/* chek if string is a number (can be double also) */
int is_number(const char *s, double *out) {
    if (s == NULL) return FALSE;

    // skip space at the start
    while (isspace((unsigned char)*s)) s++;
    if (*s == NULL_CHAR) return FALSE; // empty str

    // sign + or -
    const char *p = s;
    if (*p == PLUS_CHAR || *p == MINUS_CHAR) p++;

    int has_digit = 0;
    int has_dot = 0;

    // loop over chars
    for (; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            has_digit = TRUE;
        } else if (*p == DOT_CHAR) {
            if (has_dot) {
                return FALSE; // more than 1 dot = invalid
            }
            has_dot = TRUE;
        } else if (isspace((unsigned char)*p)) {
            // allow space at end but nothing after
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == NULL_CHAR) break;
            return FALSE;
        } else {
            return FALSE; // not valid char
        }
    }

    if (!has_digit) return FALSE; // must be atleast 1 digit

    // if ok then convert to double
    if (out) {
        *out = strtod(s, NULL);
    }
    return TRUE;
}

/* chek if its a register (like r0 - r7) */
int is_register(const char *op) {
    char opbuf[MAX_OPERAND_LEN];
    strncpy(opbuf, op, MAX_OPERAND_LEN - 1);
    opbuf[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    // remove spaces at start
    char *reg = opbuf;
    while (isspace((unsigned char)*reg)) reg++;

    // remove spaces at end
    char *end = reg + strlen(reg) - 1;
    while (end > reg && isspace((unsigned char)*end)) *end-- = NULL_CHAR;
    *(end + 1) = NULL_CHAR;

    if (strlen(reg) < 2 || strlen(reg) > 3) {
        return FALSE; // must be r0 - r7
    }

    if (reg[0] == R_CHAR) {
        // check if second char is digit between 0-7
        if (reg[1] < R0 || reg[1] > R7) {
            return NOT_FOUND;
        }
    } else {
        return FALSE; // not a reg
    }
    return TRUE;
}

/* chek if its an immidiate value (#something) */
int is_immediate(const char *op) {
    char opbuf[MAX_OPERAND_LEN];
    strncpy(opbuf, op, MAX_OPERAND_LEN - 1);
    opbuf[MAX_OPERAND_LEN - 1] = NULL_CHAR;

    // skip leading space
    char *reg = opbuf;
    while (isspace((unsigned char)*reg)) reg++;

    // first char must be '#'
    if (reg[0] != IMMEDIATE_CHAR) {
        return 0;
    }
    return 1;
}

/* chek if its a matrix like [2][3] or mat[4][5] */
int is_matrix(const char *op) {
    char buf[MAX_OPERAND_LEN];
    strncpy(buf, op, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = NULL_CHAR;

    int valid = 0;

    // first '['
    char *first_bracket = strchr(buf, SQUARE_BRACKET_START_CHAR);
    if (!first_bracket) return NOT_FOUND;

    // first ']'
    char *first_close = strchr(first_bracket + 1, SQUARE_BRACKET_END_CHAR);
    if (!first_close) return NOT_FOUND;

    // extract first value
    *first_close = NULL_CHAR;
    char *first_num_str = first_bracket + 1;

    double first_value;
    if (!is_number(first_num_str, &first_value) && is_register(first_num_str) != TRUE) {
        valid = FALSE_FOUND;
    }

    // second '['
    char *second_bracket = strchr(first_close + 1, SQUARE_BRACKET_START_CHAR);
    if (!second_bracket) return NOT_FOUND;

    // second ']'
    char *second_close = strchr(second_bracket, SQUARE_BRACKET_END_CHAR);
    if (!second_close) return NOT_FOUND;

    // extract second value
    *second_close = NULL_CHAR;
    char *second_num_str = second_bracket + 1;

    double second_value;
    if (!is_number(second_num_str, &second_value) && is_register(second_num_str) != TRUE) {
        valid = FALSE_FOUND;
    }

    // if 1 is reg and other is number its invalid
    if (is_register(first_num_str) != is_register(second_num_str)) {
        return FALSE_FOUND;
    }

    // if both numbers are whole int
    if (first_value == trunc(first_value) && second_value == trunc(second_value)) {
        return (int)first_value * (int)second_value;
    }

    return valid;
}

/* print error msg to stderr */
void print_error(const char *filename, int line_number, const char *msg) {
    fprintf(stderr, "Error: %s at line %d: %s\n", filename, line_number, msg);
}
