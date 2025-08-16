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

static inline void error_at_row(const char *filename, const Row *row, const char *msg) {
    print_error(filename, row ? (int)row->original_line_number : -1, msg);
}

// --- unchanged signature ---
unsigned int encode_command_line(Row *row)
{
    int src_mode = 0, dest_mode = 0;
    char operands_copy[MAX_OPERAND_LEN];
    char *src = NULL, *dest = NULL;

    strncpy(operands_copy, row->operands_string, MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = '\0';

    src  = strtok(operands_copy, COMMA_STRING);
    dest = strtok(NULL,        COMMA_STRING);

    if (src) {
        if (is_matrix(src) != NOT_FOUND)      src_mode = MATRIX_ACCESS_ADDRESSING;
        else if (src[0] == IMMEDIATE_CHAR)    src_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(src))            src_mode = DIRECT_REGISTER_ADDRESSING;
        else                                  src_mode = DIRECT_ADDRESSING;  // label/direct by default
    }

    if (dest) {
        if (is_matrix(dest) != NOT_FOUND)     dest_mode = MATRIX_ACCESS_ADDRESSING;
        else if (dest[0] == IMMEDIATE_CHAR)   dest_mode = IMMEDIATE_ADDRESSING;
        else if (is_register(dest))           dest_mode = DIRECT_REGISTER_ADDRESSING;
        else                                  dest_mode = DIRECT_ADDRESSING;
    }

    /* --- special case: one operand that is a label (dest is NULL) ---
       Requirement: put src mode = 0 and dest mode = DIRECT_ADDRESSING. */
    if (src && !dest && src_mode == DIRECT_ADDRESSING) {
        src_mode  = 0;                        // “no src” / 00
        dest_mode = DIRECT_ADDRESSING;        // operand treated as dest
    }

    unsigned int binary = 0;
    binary |= (row->command & 0xF) << 6;      // bits 6–9: opcode
    binary |= (src_mode  & 0x3) << 4;         // bits 4–5: src mode
    binary |= (dest_mode & 0x3) << 2;         // bits 2–3: dest mode
    binary |= A_ARE;                          // bits 0–1: ARE=00 (Absolute) for first word
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

// main function updated to report errors with filename + original line
int encode_operand_row(Row *row, Labels *labels, const char *src_filename)
{
    if (!row) return FALSE;

    const char *operand = row->operands_string;
    if (!operand || !*operand) {
        error_at_row(src_filename, row, "Missing operand text for encoding");
        return FALSE;
    }

    // 1) Two-register special form: "rX, rY" (spaces optional)
    {
        int r1 = -1, r2 = -1;
        if (sscanf(operand, " r%d , r%d ", &r1, &r2) == 2 ||
            sscanf(operand, " r%d ,%d ",  &r1, &r2) == 2 ||
            sscanf(operand, " r%d,%d ",   &r1, &r2) == 2) {
            if (r1 >= 0 && r1 <= 15 && r2 >= 0 && r2 <= 15) {
                unsigned int word = ((r1 & 0xF) << 6) | ((r2 & 0xF) << 2) | A_ARE;
                row->binary_machine_code = (word & TEN_BIT_MASK);
                return TRUE;
            }
            error_at_row(src_filename, row, "Register index out of range in two-register operand");
            return FALSE;
        }
    }

    // 2) Matrix operand (this encodes the REGISTERS word from something like "M1[r2][r7]")
    if (is_matrix(operand) != NOT_FOUND) {
        const char *br = strchr(operand, '[');
        if (!br) {
            error_at_row(src_filename, row, "Matrix operand missing register brackets");
            return FALSE;
        }
        unsigned int regs_word = encode_matrix_regs(br);
        if (!regs_word) {
            error_at_row(src_filename, row, "Invalid matrix register pattern; expected [rX][rY]");
            return FALSE;
        }
        row->binary_machine_code = regs_word & TEN_BIT_MASK;
        return TRUE;
    }

    // 3) Immediate: "#value"
    if (operand[0] == IMMEDIATE_CHAR) {
        char *endp = NULL;
        long v = strtol(&operand[1], &endp, 10);
        if (&operand[1] == endp) {
            error_at_row(src_filename, row, "Invalid immediate literal after '#'");
            return FALSE;
        }
        // store low 8 bits in payload
        unsigned int payload = (unsigned int)(v & 0xFF);
        row->binary_machine_code = pack_payload_with_are(payload, A_ARE) & TEN_BIT_MASK;
        return TRUE;
    }

    // 4) Single register with role hint in row->binary_machine_code (1=src, 2=dest)
    if (is_register(operand)) {
        int regnum = atoi(&operand[1]);
        if (regnum < 0 || regnum > 7) {
            error_at_row(src_filename, row, "Register index out of range (expected r0..r7)");
            return FALSE;
        }
        unsigned int role_hint = row->binary_machine_code; // caller puts 1 or 2 here beforehand
        unsigned int word = 0;

        if (role_hint == 1) {          // source
            word = ((unsigned int)regnum << 6) | A_ARE;
        } else if (role_hint == 2) {   // dest
            word = ((unsigned int)regnum << 2) | A_ARE;
        } else {
            error_at_row(src_filename, row, "Missing role hint for single-register operand");
            return FALSE;
        }

        row->binary_machine_code = (word & TEN_BIT_MASK);
        return TRUE;
    }

    // 5) Label (direct addressing): address -> bits 2–9; ARE=10 (reloc) or 01 (ext)
    {
        Label *lbl = find_label_by_name(labels, operand);
        if (!lbl) {
            error_at_row(src_filename, row, "Label not found");
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

int encode_data_row(Row *row, Labels *labels, const char *src_filename)
{
    (void)labels;

    if (row->command == STR) {
        if (!row->operands_string) {
            error_at_row(src_filename, row, ".string with empty payload");
            return FALSE;
        }
        if (row->operands_string[0] == '\0') {
            row->binary_machine_code = 0; // empty string, encode as zero
            return TRUE;
        }
        // NOTE: This only encodes the first character here; full .string expansion
        // should be handled by table expansion logic outside this function.
        row->binary_machine_code = ((unsigned char)row->operands_string[0]) & TEN_BIT_MASK;
        return TRUE;
    }

    if (row->command == DAT || row->command == MAT) {
        double val;
        if (is_number(row->operands_string, &val)) {
            long iv = (long)val;
            // 10-bit two's complement for data words
            iv &= TEN_BIT_MASK;
            row->binary_machine_code = (unsigned int)iv;
            return TRUE;
        } else {
            error_at_row(src_filename, row, "Invalid numeric literal in data directive");
            return FALSE;
        }
    }

    error_at_row(src_filename, row, "Unsupported data directive for encoding");
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

int parse_table_to_binary(Table *table, Labels *labels, const char *src_filename)
{
    int i;
    int had_error = FALSE;

    for (i = 0; i < table->size; ++i) {
        Row *row = get_row(table, i);

        if (row->is_command_line && row->command < NUMBER_OF_COMMANDS) {
            // 1) First word: opcode + modes + ARE=00
            row->binary_machine_code = encode_command_line(row);
        }
        else {
            int ok = FALSE;
            if (row->command < NUMBER_OF_COMMANDS) {
                ok = encode_operand_row(row, labels, src_filename);
            }
            else {
                ok = encode_data_row(row, labels, src_filename);
            }
            if (!ok) {
                // ensure we record an error but continue to process the rest
                had_error = TRUE;
            }
        }
    }

    return had_error ? FALSE : TRUE;
}
