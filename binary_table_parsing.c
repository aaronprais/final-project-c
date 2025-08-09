#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "binary_table_parsing.h"
#include "table.h"
#include "labels.h"
#include "util.h"

// Purpose: Encodes the main command line into a 10-bit binary word.
//          This includes the opcode (command), source and destination addressing modes.
// Input:   Row *row - a pointer to a row containing command and operands string.
// Output:  unsigned int - the binary encoded 10-bit command line.
unsigned int encode_command_line(Row *row)
{
    int src_mode = 0, dest_mode = 0;
    char operands_copy[MAX_OPERAND_LEN];
    char *src = NULL, *dest = NULL;

    // Copy operand string to a temporary buffer to safely tokenize
    strncpy(operands_copy, row->operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = '\0'; // Ensure null termination

    // Extract source and destination operands using strtok
    src = strtok(operands_copy, COMMA_STRING);
    dest = strtok(NULL, COMMA_STRING);

    // Determine addressing mode for source operand
    if (src)
    {
        if (is_matrix(src) != NOT_FOUND)
            src_mode = MATRIX_ACCESS_ADDRESSING;
        else if (src[0] == IMMEDIATE_CHAR)
            src_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(src))
            src_mode = DIRECT_REGISTER_ADDRESSING;
        else
            src_mode = DIRECT_ADDRESSING; // Direct (label)
    }

    // Determine addressing mode for destination operand
    if (dest)
    {
        if (is_matrix(dest) != NOT_FOUND)
            dest_mode = MATRIX_ACCESS_ADDRESSING;
        else if (dest[0] == IMMEDIATE_CHAR)
            dest_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(dest))
            dest_mode = DIRECT_REGISTER_ADDRESSING;
        else
            dest_mode = DIRECT_ADDRESSING; // Direct (label)
    }

    // Begin constructing the 10-bit binary word
    unsigned int binary = 0;

    // Insert command (opcode) in bits 6-9
    binary |= (row->command & 0xF) << 6;

    // Insert source mode in bits 4-5
    binary |= (src_mode & 0x3) << 4;

    // Insert destination mode in bits 2-3
    binary |= (dest_mode & 0x3) << 2;

    // Last 2 bits are ARE bits, initialized to 00
    binary |= 0b00;

    return binary;
}


// Purpose: Encodes a matrix operand consisting of two registers (e.g., [r2][r7]) into a 10-bit binary word.
unsigned int encode_registers(const char *regstr)
{
    int reg1 = -1, reg2 = -1;

    // Parse the input string of format "[rX][rY]" into two register numbers
    sscanf(regstr, "[r%d][r%d]", &reg1, &reg2);

    // If both registers are valid (0–7), encode them
    if (reg1 >= 0 && reg1 <= 7 && reg2 >= 0 && reg2 <= 7)
    {
        // Shift reg1 to bits 6–8, reg2 to bits 2–4, leave bits 0–1 as 0 (ARE)
        return ((reg1 & 0x7) << 6) | ((reg2 & 0x7) << 2) | 0b00;
    }

    // If registers are invalid, return 0 (error fallback)
    return 0;
}


// Purpose: Encodes a single register operand into either the source or destination bits.
unsigned int encode_register(const char *regstr, int is_source)
{
    // Extract the register number from the string (skip 'r' character)
    int reg = atoi(&regstr[1]);

    // Encode the register into bits 6-8 if source, or 2-4 if destination
    return is_source ? ((reg & 0x7) << 6) : ((reg & 0x7) << 2);
}

// Purpose: Encodes a label operand into its 10-bit binary representation.
unsigned int encode_label(Labels *labels, const char *label)
{
    // Look up the label in the label table
    Label *lbl = find_label_by_name(labels, label);

    // If label doesn't exist, print error and return 0
    if (!lbl)
    {
        printf("Error: label '%s' not found in table.\n", label);
        return 0;
    }

    // Extract address and determine ARE bits (01 for EXT, 10 otherwise)
    unsigned int address = lbl->decimal_address;
    unsigned int are = (lbl->type == EXT) ? 0b01 : 0b10;

    // Combine address and ARE bits: address in bits 2–9, ARE in bits 0–1
    return ((address & 0x3FF) << 2) | (are & 0x3);
}


// Purpose: Encodes two register operands (source and destination) into one 10-bit binary word.
unsigned int encode_two_registers(const char *src, const char *dest)
{
    int r1 = atoi(&src[1]);
    int r2 = atoi(&dest[1]);
    return ((r1 & 0x7) << 6) | ((r2 & 0x7) << 2) | 0b00;
}


// Purpose: Encodes an immediate operand (e.g., "#5" or "#-3") into a 10-bit binary word.
unsigned int encode_immediate(const char *immstr)
{
    int value = atoi(&immstr[1]);

    // Convert to 10-bit two's complement range
    if (value < 0) {
        value = (1 << 10) + value;
    }
    return (unsigned int)(value & 0x3FF);
}


// Purpose: Encodes data rows such as .string, .data, and .mat directives into their binary representation.
int encode_data_row(Row *row, Labels *labels)
{
    (void)labels; // unused for now

    if (row->command == STR)
    {
        row->binary_machine_code = (unsigned char)row->operands_string[0];
        return TRUE;
    }

    if (row->command == DAT || row->command == MAT)
    {
        double val;
        if (is_number(row->operands_string, &val))
        {
            int int_val = (int)val;
            if (int_val < 0) int_val = (1 << 10) + int_val;
            row->binary_machine_code = (unsigned int)(int_val & 0x3FF);
            return TRUE;
        }
    }
    return FALSE;
}


// Purpose: Determines how many extra binary words are needed for a given operand string
int how_many_extra_words(const char *operands_string)
{
    char copy[MAX_OPERAND_LEN];
    char *src = NULL, *dest = NULL;

    strncpy(copy, operands_string, MAX_OPERAND_LEN - 1);
    copy[MAX_OPERAND_LEN - 1] = '\0';

    src = strtok(copy, COMMA_STRING);
    dest = strtok(NULL, COMMA_STRING);

    if (src && dest)
    {
        if (is_register(src) && is_register(dest))
            return 1;
        return 2;
    }
    else if (src)
    {
        if (is_matrix(src) != NOT_FOUND)
            return 2;
        return 1;
    }
    return 0;
}


// Purpose: Encodes a single operand (matrix, immediate, register, or label)
unsigned int encode_operand(const char *operand, Labels *labels)
{
    if (is_matrix(operand) != NOT_FOUND)
        return encode_registers(operand);
    else if (operand[0] == IMMEDIATE_CHAR)
        return encode_immediate(operand);
    else if (is_register(operand))
        return encode_register(operand, 1);
    else
        return encode_label(labels, operand);
}


// Purpose: Removes leading and trailing whitespace characters from the given string in-place.
void trim_spaces(char *str)
{
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) *end-- = '\0';
    if (start != str) memmove(str, start, strlen(start) + 1);
}

// Purpose: Encodes each row in the assembly table into its binary machine code representation.
int parse_table_to_binary(Table *table, Labels *labels)
{
    int i;

    for (i = 0; i < table->size;)
    {
        Row *row = get_row(table, i);

        if (row->is_command_line && row->command < NUMBER_OF_COMMANDS)
        {
            row->binary_machine_code = encode_command_line(row);

            int extra_words = how_many_extra_words(row->operands_string);

            char operands_copy[MAX_OPERAND_LEN];
            char *src = NULL, *dest = NULL;
            strncpy(operands_copy, row->operands_string, MAX_OPERAND_LEN - 1);
            operands_copy[MAX_OPERAND_LEN - 1] = '\0';

            src = strtok(operands_copy, COMMA_STRING);
            dest = strtok(NULL, COMMA_STRING);

            if (src) trim_spaces(src);
            if (dest) trim_spaces(dest);

            int word_index = 1;

            if (src && dest && is_register(src) && is_register(dest))
            {
                Row *extra = get_row(table, i + word_index);
                extra->binary_machine_code = encode_two_registers(src, dest);
                word_index++;
            }
            else
            {
                if (src)
                {
                    Row *extra = get_row(table, i + word_index++);

                    if (is_matrix(src) != NOT_FOUND)
                    {
                        char matrix_label[MAX_OPERAND_LEN];
                        sscanf(src, "%[^[]", matrix_label);

                        extra->binary_machine_code = encode_label(labels, matrix_label);

                        Row *extra2 = get_row(table, i + word_index++);
                        extra2->binary_machine_code = encode_registers(src);
                    }
                    else
                    {
                        extra->binary_machine_code = encode_operand(src, labels);
                    }
                }

                if (dest)
                {
                    Row *extra = get_row(table, i + word_index++);
                    extra->binary_machine_code = encode_operand(dest, labels);
                }
            }

            i += extra_words + 1;
        }
        else
        {
            encode_data_row(row, labels);
            i++;
        }
    }
    return TRUE;
}
