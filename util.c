#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"

/*int find_command(char *word, char *label)
{
    int i;

    for (i = 0; i < NUMBER_OF_COMMANDS; i++)
    {
        if (strcmp(word, command_names[i]) == 0) {
            return i;
        }
        if (i < NUMBER_OF_DATA_TYPES &&
            strcmp(label, "") != 0 &&
            strcmp(word, command_names[i + NUMBER_OF_COMMANDS]) == 0) {
            return i;
         }
    }
    
    return NOT_FOUND;
}*/

int is_valid_number(const char *str) {
    const char *ptr = str;

    // Skip leading spaces
    while (*ptr == SPACE_CHAR) {
        ptr++;
    }

    // Check if we have at least one character
    if (*ptr == NULL_CHAR) {
        return FALSE;
    }

    // Handle optional negative sign
    if (*ptr == MINUS_CHAR) {
        ptr++;
        // Must have at least one digit after the minus sign
        if (*ptr == NULL_CHAR || !isdigit(*ptr)) {
            return FALSE;
        }
    }

    // Check for at least one digit before decimal point
    if (!isdigit(*ptr)) {
        return FALSE;
    }

    // Parse integer part
    while (isdigit(*ptr)) {
        ptr++;
    }

    // Check for decimal point
    if (*ptr == DOT_CHAR) {
        ptr++;
        // Must have at least one digit after decimal point
        if (*ptr == NULL_CHAR || !isdigit(*ptr)) {
            return FALSE;
        }
        // Parse fractional part
        while (isdigit(*ptr)) {
            ptr++;
        }
    }

    // Skip trailing spaces
    while (*ptr == SPACE_CHAR) {
        ptr++;
    }

    // Should be at end of string now
    return (*ptr == NULL_CHAR) ? TRUE : FALSE;
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
    return is_valid_number(op+1);
}

int is_matrix(const char *op) {
    return strchr(op, '[') && strstr(op, "][");
}

int is_label(const char *op, const char *lab) {
    return 0;
}