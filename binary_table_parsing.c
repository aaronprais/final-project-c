#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "binary_table_parsing.h"
#include "table.h"
#include "labels.h"
#include "util.h"

static inline void error_at_row(const char *filename, const Row *row, const char *msg) {
    print_error(filename, row ? (int)row->original_line_number : -1, msg);
}

// Helper lambdas
static inline int detect_mode(const char *op) {
    if (!op)                             return 0; /* won't be used if missing */
    if (is_matrix(op) != NOT_FOUND)      return MATRIX_ACCESS_ADDRESSING;   // 2
    if (is_immediate(op))                return IMMEDIATE_ADDRESSING;       // 0
    if (is_register(op))                 return DIRECT_REGISTER_ADDRESSING;  // 3
    return DIRECT_ADDRESSING;                                                // 1 (label/direct by default)
}
static inline const char* mode_name(int m) {
    switch (m) {
        case IMMEDIATE_ADDRESSING:       return "Immediate";
        case DIRECT_ADDRESSING:          return "Direct";
        case MATRIX_ACCESS_ADDRESSING:   return "Matrix";
        case DIRECT_REGISTER_ADDRESSING: return "Register";
        default:                         return "Unknown";
    }
}

// --- updated signature & implementation ---
// Returns TRUE on success, FALSE on any validation/encoding error.
// On success, writes first-word encoding into row->binary_machine_code.
int encode_command_line(Row *row, const char *src_filename)
{
    if (!row) return FALSE;

    // Parse raw operands (at most two, separated by a comma)
    int src_mode = 0, dest_mode = 0;
    char operands_copy[MAX_OPERAND_LEN];
    char *src_tok = NULL, *dest_tok = NULL;

    strncpy(operands_copy, row->operands_string ? row->operands_string : "", MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = '\0';

    // Tokenize by comma
    src_tok  = strtok(operands_copy, COMMA_STRING);
    dest_tok = strtok(NULL,          COMMA_STRING);

    // Compute preliminary modes for whatever tokens we have
    if (src_tok)  src_mode  = detect_mode(src_tok);
    if (dest_tok) dest_mode = detect_mode(dest_tok);

    // Figure out expected operand count & allowed addressing masks per opcode.
    // Bitmask uses bit i = 1 if addressing mode i (0..3) is allowed.
    // Legend: 0=imm, 1=direct, 2=matrix, 3=reg
    const unsigned src_all   = 0xF; // 0|1|2|3
    const unsigned dest_all  = 0xF;
    const unsigned not_immediate = 0xE; // 1|2|3
    const unsigned lea_src   = 0x6; // 1|2 (Direct, Matrix)

    int cmd = row->command;
    int expected_operands = 2;       // default: most binary ops have 2
    unsigned allow_src = 0, allow_dst = 0;

    switch (cmd) {
        case MOV: /* 0 */ allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;
        case CMP: /* 1 */ allow_src = src_all;        allow_dst = dest_all;      expected_operands = 2; break;
        case ADD: /* 2 */ allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;
        case SUB: /* 3 */ allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;

        case NOT: /* 4 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case CLR: /* 5 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case LEA: /* 6 */ allow_src = lea_src;        allow_dst = not_immediate; expected_operands = 2; break;
        case INC: /* 7 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case DEC: /* 8 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;

        case JMP: /* 9 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case BNE: /*10 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case RED: /*11 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case PRN: /*12 */ allow_src = 0;              allow_dst = dest_all;      expected_operands = 1; break;
        case JSR: /*13 */ allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;

        case RTS: /*14 */ allow_src = 0;              allow_dst = 0;             expected_operands = 0; break;
        case STP: /*15 */ allow_src = 0;              allow_dst = 0;             expected_operands = 0; break;

        default:
            error_at_row(src_filename, row, "Unknown command opcode");
            return FALSE;
    }

    // Normalize single-operand syntax: if exactly one token was provided,
    // treat it as the *destination* operand for 1-operand commands.
    // (For 2-operand commands, a single operand is an error.)
    int provided_operands = (src_tok ? 1 : 0) + (dest_tok ? 1 : 0);

    if (expected_operands == 1) {
        if (provided_operands == 0) {
            print_error(src_filename, (int)row->original_line_number, "Expected one operand, got none");
            return FALSE;
        }
        if (provided_operands > 1) {
            print_error(src_filename, (int)row->original_line_number, "Expected one operand, got two");
            return FALSE;
        }
        // Move the single operand into "dest"
        if (src_tok && !dest_tok) {
            dest_tok  = src_tok;
            dest_mode = src_mode;
            src_tok   = NULL;
            src_mode  = 0; // “no src”
        }
    } else if (expected_operands == 2) {
        if (provided_operands < 2) {
            print_error(src_filename, (int)row->original_line_number, "Expected two operands, got one");
            return FALSE;
        }
        if (provided_operands > 2) {
            print_error(src_filename, (int)row->original_line_number, "Expected two operands, got more than two");
            return FALSE;
        }
    } else { // expected_operands == 0
        if (provided_operands != 0) {
            print_error(src_filename, (int)row->original_line_number, "Expected no operands for this instruction");
            return FALSE;
        }
    }

    // Validate addressing modes against the allowed masks
    if (expected_operands == 2) {
        if (((allow_src >> src_mode) & 1U) == 0U) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Invalid SOURCE addressing for opcode %d: %s", cmd, mode_name(src_mode));
            error_at_row(src_filename, row, buf);
            return FALSE;
        }
    }
    if (expected_operands >= 1) {
        if (((allow_dst >> dest_mode) & 1U) == 0U) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Invalid DESTINATION addressing for opcode %d: %s", cmd, mode_name(dest_mode));
            error_at_row(src_filename, row, buf);
            return FALSE;
        }
    }

    // Passed validation — build the first word
    unsigned int binary = 0;
    binary |= (row->command & 0xF) << 6;   // bits 6–9: opcode
    binary |= (src_tok  ? (src_mode  & 0x3) << 4 : 0);  // bits 4–5: src mode (00 if none)
    binary |= (dest_tok ? (dest_mode & 0x3) << 2 : 0);  // bits 2–3: dest mode (00 if none)
    binary |= A_ARE;                       // bits 0–1: ARE=00 (Absolute) for first word

    row->binary_machine_code = (binary & TEN_BIT_MASK);
    return TRUE;
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
        return ((r_row & 0x7) << 6) | ((r_col & 0x7) << 2) | A_ARE;
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
            sscanf(operand, " r%d ,r%d ",  &r1, &r2) == 2 ||
            sscanf(operand, " r%d,r%d ",   &r1, &r2) == 2) {
            if (r1 >= 0 && r1 <= 7 && r2 >= 0 && r2 <= 7) {
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
    if (is_immediate(operand)) {
        double val = 0.0;
        if (!is_number(operand + 1, &val)) {
            print_error(src_filename, (int)row->original_line_number,"Invalid immediate operand; expected # followed by a number");
            return FALSE;
        }
        // store low 8 bits in payload
        unsigned int payload = (unsigned int)((int)val & 0xFF);
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
            if (encode_command_line(row, src_filename) == FALSE) {
                // ensure we record an error but continue to process the rest
                had_error = TRUE;
            }
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
