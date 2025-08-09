#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "binary_table_parsing.h"
#include "table.h"
#include "labels.h"
#include "util.h"

// ===================== CONSTANT DEFINITIONS =====================
// MAX_OPERAND_LEN defines the maximum allowed length for an operand string.
// This is used for safe memory operations (e.g., copying operand strings),
// preventing buffer overflows, and ensuring consistency throughout operand parsing logic.
#define MAX_OPERAND_LEN 100

// NUMBER_OF_COMMANDS defines how many machine-supported commands (opcodes)
// exist in the system. This does NOT include directives like .data, .string, .mat,
// which are handled separately.
#define NUMBER_OF_COMMANDS 16


// Purpose: Encodes the main command line into a 10-bit binary word.
//          This includes the opcode (command), source and destination addressing modes.
// Input:   Row *row - a pointer to a row containing command and operands string.
// Output:  unsigned int - the binary encoded 10-bit command line.
unsigned int encode_command_line(Row *row)
{
    // Initialize addressing modes to default value 0
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
            src_mode = 2; // Matrix
        else if (src[0] == IMMEDIATE_CHAR)
            src_mode = 0; // Immediate
        else if (is_register(src))
            src_mode = 3; // Register
        else
            src_mode = 1; // Direct (label)
    }

    // Determine addressing mode for destination operand
    if (dest)
    {
        if (is_matrix(dest) != NOT_FOUND)
            dest_mode = 2; // Matrix
        else if (dest[0] == IMMEDIATE_CHAR)
            dest_mode = 0; // Immediate
        else if (is_register(dest))
            dest_mode = 3; // Register
        else
            dest_mode = 1; // Direct (label)
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
// Input:   const char *regstr - the string representing two registers inside brackets.
// Output:  unsigned int - 10-bit binary encoding of both registers.
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
// Inputs:  const char *regstr - the register string (e.g., "r3")
//          int is_source - boolean indicating if it's a source (1) or destination (0)
// Output:  unsigned int - binary representation of the register shifted into position
unsigned int encode_register(const char *regstr, int is_source)
{
    // Extract the register number from the string (skip 'r' character)
    int reg = atoi(&regstr[1]);

    // Encode the register into bits 6-8 if source, or 2-4 if destination
    return is_source ? ((reg & 0x7) << 6) : ((reg & 0x7) << 2);
}

 // Purpose: Encodes a label operand (e.g., "LOOP") into its 10-bit binary representation.
//          The address and ARE bits are derived from the label information stored in the labels table.
// Input:   Labels *labels - pointer to the labels table
//          const char *label - name of the label to encode
// Output:  unsigned int - encoded address + ARE as 10-bit binary
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
    return ((address & 0xFFF) << 2) | are;
}


// Purpose: Encodes two register operands (source and destination) into one 10-bit binary word.
//          This is used when both operands in the instruction are registers, allowing for compact encoding.
// Input:   const char *src  - the source register string (e.g., "r2")
//          const char *dest - the destination register string (e.g., "r5")
// Output:  unsigned int - binary encoding where:
//          - bits 6–8 represent the source register (rX)
//          - bits 2–4 represent the destination register (rY)
//          - bits 0–1 (ARE bits) are set to 00
unsigned int encode_two_registers(const char *src, const char *dest)
{
    // Convert source register string (e.g., "r3") to integer (e.g., 3)
    int r1 = atoi(&src[1]);

    // Convert destination register string (e.g., "r5") to integer (e.g., 5)
    int r2 = atoi(&dest[1]);

    // Encode r1 into bits 6–8, r2 into bits 2–4, and set ARE bits (0–1) to 00
    return ((r1 & 0x7) << 6) | ((r2 & 0x7) << 2) | 0b00;
}


// Purpose: Encodes an immediate operand (e.g., "#5" or "#-3") into a 10-bit binary word.
//          Handles both positive and negative values by converting negatives to two's complement.
// Input:   const char *immstr - the immediate value as a string, starting with '#'
// Output:  unsigned int - a 10-bit binary representation of the immediate value
unsigned int encode_immediate(const char *immstr)
{
    // Convert the numeric part of the immediate string to an integer
    // Skip the '#' prefix by using &immstr[1]
    int value = atoi(&immstr[1]);

    // If the value is negative, convert to two's complement using 12 bits
    if (value < 0)
        value = (1 << 12) + value;

    // Return the lower 12 bits (ensures result is within range)
    return (unsigned int)(value & 0xFFF);
}


// Purpose: Encodes data rows such as .string, .data, and .mat directives into their binary representation.
//          - For .string (STR), encodes the ASCII value of the first character.
//          - For .data and .mat, encodes numeric values including negative values (using 2's complement).
// Input:   Row *row     - a pointer to the current table row
//          Labels *labels - (currently unused but passed for interface consistency)
// Output:  int - TRUE if the row was successfully encoded, FALSE otherwise
int encode_data_row(Row *row, Labels *labels)
{
    // Check if the row is a .string directive (STR)
    if (row->command == STR)
    {
        // Encode the ASCII value of the first character in the string
        row->binary_machine_code = (row->operands_string[0] & 0xFF);
        return TRUE;
    }

    // Check if the row is a .data or .mat directive
    if (row->command == DAT || row->command == MAT)
    {
        double val;

        // Check if the operand is a valid number and convert it to a double
        if (is_number(row->operands_string, &val))
        {
            int int_val = (int)val;

            // If the value is negative, convert it to two's complement (12-bit)
            if (int_val < 0)
                int_val = (1 << 12) + int_val;

            // Store the lower 12 bits of the result into the binary code field
            row->binary_machine_code = int_val & 0xFFF;
            return TRUE;
        }
    }
    // If no encoding was performed, return FALSE
    return FALSE;
}


// Purpose: Determines how many extra binary words (machine code lines) are needed for a given operand string in a command line.
//          The number of extra words depends on whether the operands are registers, matrices, labels, or immediates.
// Input:   const char *operands_string - the full operands string of a command, e.g., "r2,STR" or "M1[r2][r7],W"
// Output:  int - number of additional binary words needed (0, 1, or 2)
int how_many_extra_words(const char *operands_string)
{
    // Copy the original operand string to a temporary buffer
    char copy[MAX_OPERAND_LEN];
    char *src = NULL, *dest = NULL;

    // Copy the operand string safely into 'copy' buffer (with null termination)
    strncpy(copy, operands_string, MAX_OPERAND_LEN - 1);
    copy[MAX_OPERAND_LEN - 1] = '\0';

    // Split the operands using ',' delimiter into source and destination
    src = strtok(copy, COMMA_STRING);
    dest = strtok(NULL, COMMA_STRING);

    // If both source and destination operands exist
    if (src && dest)
    {
        // If both operands are registers, only 1 extra word is needed
        if (is_register(src) && is_register(dest))
            return 1;

        // Otherwise (e.g., label and register, or immediate and label), 2 words are needed
        return 2;
    }
    // If only a single operand exists
    else if (src)
    {
        // If it's a matrix (e.g., M1[r2][r7]), it needs 2 extra words (label + registers)
        if (is_matrix(src) != NOT_FOUND)
            return 2;

        // Any other single operand (e.g., register, immediate, or label) requires 1 word
        return 1;
    }
    // No operands means no extra words are needed
    return 0;
}


// Purpose: Encodes a single operand (matrix, immediate, register, or label) into a 12-bit binary representation.
// Input:   const char *operand - the operand string to encode (e.g., "#5", "r3", "M1[r2][r7]", "LOOP")
//          Labels *labels - pointer to the labels table for looking up label addresses
// Output:  unsigned int - the encoded binary value representing the operand
unsigned int encode_operand(const char *operand, Labels *labels)
{
    // If operand is a matrix (e.g., M1[r2][r7]), encode the register indices into binary
    if (is_matrix(operand) != NOT_FOUND)
        return encode_registers(operand);

    // If operand is an immediate value (e.g., #5 or #-3), encode it directly
    else if (operand[0] == IMMEDIATE_CHAR)
        return encode_immediate(operand);

    // If operand is a register (e.g., r1, r2, ...), encode its binary value
    else if (is_register(operand))
        return encode_register(operand, 1); // source flag can be arbitrary here

    // Otherwise, treat it as a label and look up its address for encoding
    else
        return encode_label(labels, operand);
}


// Purpose: Removes leading and trailing whitespace characters from the given string in-place.
// Input:   char *str - the string to be trimmed.
// Output:  The function modifies the input string directly; no return value.
void trim_spaces(char *str)
{
    // Initialize a pointer to the start of the string
    char *start = str;

    // Move the 'start' pointer forward to skip all leading whitespace
    while (isspace((unsigned char)*start))
        start++;

    // Initialize an 'end' pointer to the last character of the trimmed start
    char *end = start + strlen(start) - 1;

    // Move the 'end' pointer backwards to skip all trailing whitespace
    while (end > start && isspace((unsigned char)*end))
        *end-- = '\0'; // Replace trailing whitespace with null terminator

    // If 'start' moved (i.e., leading spaces were trimmed), shift string to beginning
    if (start != str)
        memmove(str, start, strlen(start) + 1); // +1 to include null terminator
}

// Purpose: Encodes each row in the assembly table into its binary machine code representation.
//          For command rows, it encodes the command itself and all its operands.
//          For data rows (e.g., .data, .string), it uses specific handlers.
// Inputs:  Table *table - the table containing all parsed assembly rows.
//          Labels *labels - the table of defined labels for resolving label addresses.
// Output:  Returns TRUE after encoding all rows
int parse_table_to_binary(Table *table, Labels *labels)
{
    for (int i = 0; i < table->size;)
    {
        Row *row = get_row(table, i); // Get current row from the table

        // Check if this row is a command line and within valid command range
        if (row->is_command_line && row->command < NUMBER_OF_COMMANDS)
        {
            // Encode the command itself (opcode and addressing modes)
            row->binary_machine_code = encode_command_line(row);

            // Determine how many extra words are required for operands
            int extra_words = how_many_extra_words(row->operands_string);

            // Create a copy of the operands string to tokenize safely
            char operands_copy[MAX_OPERAND_LEN];
            char *src = NULL, *dest = NULL;
            strncpy(operands_copy, row->operands_string, MAX_OPERAND_LEN - 1);
            operands_copy[MAX_OPERAND_LEN - 1] = '\0';

            // Split operands by comma into src and dest
            src = strtok(operands_copy, COMMA_STRING);
            dest = strtok(NULL, COMMA_STRING);

            // Trim spaces from operands if present
            if (src) trim_spaces(src);
            if (dest) trim_spaces(dest);

            int word_index = 1; // Start from the row after the command row

            // Case: both operands are registers (only one word needed)
            if (src && dest && is_register(src) && is_register(dest))
            {
                Row *extra = get_row(table, i + word_index);
                extra->binary_machine_code = encode_two_registers(src, dest);
                word_index++;
            }
            else // Other cases: encode each operand separately
            {
                // Handle source operand
                if (src)
                {
                    Row *extra = get_row(table, i + word_index++);

                    // If source is matrix, encode its label and registers separately
                    if (is_matrix(src) != NOT_FOUND)
                    {
                        char matrix_label[MAX_OPERAND_LEN];
                        sscanf(src, "%[^[]", matrix_label); // Extract label before [

                        extra->binary_machine_code = encode_label(labels, matrix_label);

                        Row *extra2 = get_row(table, i + word_index++);
                        extra2->binary_machine_code = encode_registers(src);
                    }
                    else
                    {
                        // Standard operand encoding
                        extra->binary_machine_code = encode_operand(src, labels);
                    }
                }

                // Handle destination operand
                if (dest)
                {
                    Row *extra = get_row(table, i + word_index++);
                    extra->binary_machine_code = encode_operand(dest, labels);
                }
            }

            // Skip to the next instruction (command + extra words)
            i += extra_words + 1;
        }
        else
        {
            // This is not a command row - encode it as a data row (.data, .string, .mat)
            encode_data_row(row, labels);
            i++;
        }
    }
    return TRUE; // Encoding successful
}