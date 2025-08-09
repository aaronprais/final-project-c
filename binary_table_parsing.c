#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "binary_table_parsing.h"
#include "table.h"
#include "labels.h"
#include "util.h"

// --- fixes / helpers at top ---
#define TEN_BIT_MASK 0x3FF
#define E_ARE 0x1
#define R_ARE 0x2
#define A_ARE 0x0

// --- unchanged signature ---
unsigned int encode_command_line(Row *row)
{
    int src_mode = 0, dest_mode = 0;
    char operands_copy[MAX_OPERAND_LEN];
    char *src = NULL, *dest = NULL;

    strncpy(operands_copy, row->operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = '\0';

    src = strtok(operands_copy, COMMA_STRING);
    dest = strtok(NULL, COMMA_STRING);

    if (src) {
        if (is_matrix(src) != NOT_FOUND)      src_mode = MATRIX_ACCESS_ADDRESSING;
        else if (src[0] == IMMEDIATE_CHAR)    src_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(src))            src_mode = DIRECT_REGISTER_ADDRESSING;
        else                                  src_mode = DIRECT_ADDRESSING;
    }

    if (dest) {
        if (is_matrix(dest) != NOT_FOUND)     dest_mode = MATRIX_ACCESS_ADDRESSING;
        else if (dest[0] == IMMEDIATE_CHAR)   dest_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(dest))           dest_mode = DIRECT_REGISTER_ADDRESSING;
        else                                  dest_mode = DIRECT_ADDRESSING;
    }

    unsigned int binary = 0;
    binary |= (row->command & 0xF) << 6;   // bits 6–9: opcode
    binary |= (src_mode  & 0x3) << 4;      // bits 4–5: src mode
    binary |= (dest_mode & 0x3) << 2;      // bits 2–3: dest mode
    binary |= A_ARE;                        // bits 0–1: ARE=00 (Absolute) for first word
    return (binary & TEN_BIT_MASK);
}

static inline unsigned int pack_payload_with_are(unsigned int payload8, unsigned int are2) {
    // payload goes to bits 2–9, ARE in bits 0–1
    return ((payload8 & 0xFF) << 2) | (are2 & 0x3);
}

// encode the registers portion of a matrix like "[r2][r7]" (row -> bits 2–5, col -> bits 6–9)
static inline unsigned int encode_matrix_regs(const char *regstr) {
    int r_row = -1, r_col = -1;
    if (sscanf(regstr, "[r%d][r%d]", &r_row, &r_col) == 2 &&
        r_row >= 0 && r_row <= 7 && r_col >= 0 && r_col <= 7) {
        return ((r_col & 0x7) << 6) | ((r_row & 0x7) << 2) | A_ARE;
    }
    return 0; // invalid pattern
}

// main function you asked for
int encode_operand_row(Row *row, Labels *labels)
{
    if (!row) return FALSE;

    // we expect the operand text for THIS extra word in row->operands_string
    const char *operand = row->operands_string;
    if (!operand || !*operand) return FALSE;

    // 1) Two-register special form: "rX, rY" (spaces optional)
    //    Packs rX -> bits 6–9, rY -> bits 2–5, ARE=00
    {
        int r1 = -1, r2 = -1;
        // allow spaces: "r1, r2" / "r1,r2"
        // (two scans to be tolerant of formats)
        if (sscanf(operand, " r%d , r%d ", &r1, &r2) == 2 ||
            sscanf(operand, " r%d ,%d ", &r1, &r2) == 2 ||
            sscanf(operand, " r%d,%d ", &r1, &r2) == 2) {
            if (r1 >= 0 && r1 <= 15 && r2 >= 0 && r2 <= 15) {
                unsigned int word = ((r1 & 0xF) << 6) | ((r2 & 0xF) << 2) | A_ARE;
                row->binary_machine_code = (word & TEN_BIT_MASK);
                return TRUE;
            }
            return FALSE;
        }
    }

    // 2) Matrix operand (this encodes the REGISTERS word from something like "M1[r2][r7]")
    if (is_matrix(operand) != NOT_FOUND) {
        unsigned int regs_word = encode_matrix_regs(strchr(operand, '[')); // from first '['
        if (!regs_word) return FALSE;
        row->binary_machine_code = regs_word & TEN_BIT_MASK;
        return TRUE;
    }

    // 3) Immediate: "#value" -> payload in bits 2–9, ARE=00
    if (operand[0] == IMMEDIATE_CHAR) {
        int v = atoi(&operand[1]);        // after '#'
        if (v < 0) v = (1 << 8) + (v & 0xFF);  // clamp to 8-bit two's complement
        row->binary_machine_code = pack_payload_with_are((unsigned int)v, A_ARE) & TEN_BIT_MASK;
        return TRUE;
        }

    // 4) Single register: use row->binary_machine_code (preexisting) as ROLE HINT
    //    1 => source (bits 6–9), 2 => dest (bits 2–5)
    if (is_register(operand)) {
        int reg = atoi(&operand[1]) & 0x7;  // 0..7
        unsigned int role_hint = row->binary_machine_code; // caller puts 1 or 2 here beforehand
        unsigned int word = 0;

        if (role_hint == 1) {          // source
            word = (reg << 6) | A_ARE;
        } else if (role_hint == 2) {   // dest
            word = (reg << 2) | A_ARE;
        } else {
            // no role info; cannot place the 3 bits in the correct 4-bit slot
            return FALSE;
        }

        row->binary_machine_code = (word & TEN_BIT_MASK);
        return TRUE;
    }

    // 5) Label (direct addressing): address goes to bits 2–9; ARE=10 (reloc) or 01 (ext)
    {
        Label *lbl = find_label_by_name(labels, operand);
        if (!lbl) {
            printf("Error: label doesn't exist");
            return FALSE;
        }
        if (lbl->type == EXT) {
            unsigned int addr8 = 0 & 0xFF;
            row->binary_machine_code = pack_payload_with_are(addr8, E_ARE) & TEN_BIT_MASK;
        }
        else {
            unsigned int addr8 = (unsigned int)lbl->decimal_address & 0xFF;
            row->binary_machine_code = pack_payload_with_are(addr8, R_ARE) & TEN_BIT_MASK;
        }
        return TRUE;
    }
}

int encode_data_row(Row *row, Labels *labels)
{
    (void)labels;

    if (row->command == STR) {
        // NOTE: This only encodes the first character here; full .string expansion
        // should be handled by table expansion logic outside this function.
        row->binary_machine_code = ((unsigned char)row->operands_string[0]) & TEN_BIT_MASK;
        return TRUE;
    }

    if (row->command == DAT || row->command == MAT) {
        double val;
        if (is_number(row->operands_string, &val)) {
            int iv = (int)val;
            if (iv < 0) iv = (1 << 10) + iv; // 10-bit two's complement for data words
            row->binary_machine_code = (unsigned int)(iv & TEN_BIT_MASK);
            return TRUE;
        }
    }
    return FALSE;
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

int parse_table_to_binary(Table *table, Labels *labels)
{
    int i;
    for (i = 0; i < table->size; ) {
        Row *row = get_row(table, i);

        if (row->is_command_line && row->command < NUMBER_OF_COMMANDS) {
            // 1) First word: opcode + modes + ARE=00
            row->binary_machine_code = encode_command_line(row);
        }
        else {
            if (row->command < NUMBER_OF_COMMANDS) {
                encode_operand_row(row, labels);
            }
            else {
                encode_data_row(row, labels);
            }
        }
        i++;
    }
    return TRUE;
}

