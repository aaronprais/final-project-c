#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "util.h"

int find_command(char *word, char *label) {
    int i;

    for (i = ZERO; i < NUMBER_OF_COMMANDS; i++) {
        if (strcmp(word, command_names[i]) == 0) {
            return i;
        }
        if (i < NUMBER_OF_DATA_TYPES &&
            strcmp(label, EMPTY_STRING) != 0 &&
            strcmp(word, command_names[i + NUMBER_OF_COMMANDS]) == 0) {
            return i + NUMBER_OF_COMMANDS;
         }
    }

    return NOT_FOUND;
}

int is_number(const char *s, double *out) {
    if (s == NULL) return FALSE;

    // skip leading spaces
    while (isspace((unsigned char)*s)) s++;
    if (*s == NULL_CHAR) return FALSE; // empty after trimming

    // optional sign
    const char *p = s;
    if (*p == PLUS_CHAR || *p == MINUS_CHAR) p++;

    int has_digit = ZERO;
    int has_dot = ZERO;

    for (; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            has_digit = TRUE;
        } else if (*p == DOT_CHAR) {
            if (has_dot) {
                // already had a dot, invalid
                return FALSE;
            }
            has_dot = TRUE;
        } else if (isspace((unsigned char)*p)) {
            // allow trailing spaces, but nothing non-space after
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == NULL_CHAR) break;
            return FALSE;
        } else {
            // invalid character
            return FALSE;
        }
    }

    if (!has_digit) return FALSE; // must contain at least one digit

    // If we reached here, it's valid
    if (out) {
        *out = strtod(s, NULL); // convert to double
    }
    return TRUE;
}

int is_register(const char *op) {
    return strlen(op) == REGISTER_LEN && op[ZERO] == R_CHAR && op[ONE] >= R0 && op[ONE] <= R7;
}

int is_immediate(const char *op) {
    // Check if first character is '#'
    if (op[0] != IMMEDIATE_CHAR) {
        return 0;
    }

    // Check if the part after '#' is a valid number
    return is_number(op++, NULL);
}

int is_matrix(const char *op) {
    char buf[MAX_OPERAND_LEN];
    strncpy(buf, op, sizeof(buf) - ONE);
    buf[sizeof(buf) - ONE] = NULL_CHAR;

    // Find the first '[' (could be at start for "[4][4]" or after name for "Mat[1][2]")
    char *first_bracket = strchr(buf, SQUARE_BRACKET_START_CHAR);
    if (!first_bracket) return NOT_FOUND;

    // Find the first ']' after the first '['
    char *first_close = strchr(first_bracket + ONE, SQUARE_BRACKET_END_CHAR);
    if (!first_close) return NOT_FOUND;

    // Extract first number (between first [ and ])
    *first_close = NULL_CHAR;  // Temporarily null-terminate
    char *first_num_str = first_bracket + ONE;  // Skip the '['

    double first_value;
    if (!is_number(first_num_str, &first_value) && !is_register(first_num_str)) {
        return NOT_FOUND;
    }

    // Find the second '[' after the first ']'
    char *second_bracket = strchr(first_close + ONE, SQUARE_BRACKET_START_CHAR);
    if (!second_bracket) return NOT_FOUND;

    // Find the second ']' after the second '['
    char *second_close = strchr(second_bracket, SQUARE_BRACKET_END_CHAR);
    if (!second_close) return NOT_FOUND;

    // Extract second number (between second [ and ])
    *second_close = NULL_CHAR;  // Temporarily null-terminate
    char *second_num_str = second_bracket + ONE;  // Skip the '['

    double second_value;
    if (!is_number(second_num_str, &second_value) && !is_register(second_num_str)) {
        return NOT_FOUND;
    }

    // Check if both values are integers (whole numbers)
    if (first_value == trunc(first_value) && second_value == trunc(second_value)) {
        return (int)first_value * (int)second_value;
    }

    return ZERO;
}

// int is_valid_label(const char *op, const char *lab) {
//     return FALSE;
// }