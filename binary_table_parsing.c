#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "binary_table_parsing.h"
#include "table.h"
#include "labels.h"
#include "util.h"

/* Small helper to print errors with original source line (very useful for users). */
static void error_at_row(const char *filename, const Row *row, const char *msg) {
    print_error(filename, row ? (int)row->original_line_number : -1, msg);
}

/* --- Addressing mode helpers --- */
static int detect_mode(const char *op) {
    if (!op)                             return 0; /* won't be used if missing */
    if (is_matrix(op) != NOT_FOUND)      return MATRIX_ACCESS_ADDRESSING;   // 2
    if (is_immediate(op))                return IMMEDIATE_ADDRESSING;       // 0
    if (is_register(op) != FALSE)        return DIRECT_REGISTER_ADDRESSING; // 3
    return DIRECT_ADDRESSING;                                                // 1
}

static const char* mode_name(int m) {
    switch (m) {
        case IMMEDIATE_ADDRESSING:       return "Immediate";
        case DIRECT_ADDRESSING:          return "Direct";
        case MATRIX_ACCESS_ADDRESSING:   return "Matrix";
        case DIRECT_REGISTER_ADDRESSING: return "Register";
        default:                         return "Unknown";
    }
}

/* Packs 8-bit payload into bits 2–9, and 2-bit ARE into bits 0–1. */
static unsigned int pack_payload_with_are(unsigned int payload8, unsigned int are2) {
    return ((payload8 & 0xFF) << 2) | (are2 & 0x3);
}

/*
 * encode_matrix_regs
 * ------------------
 * Encodes the register pair inside a matrix index like: [r2][r7]
 * Mapping: row-reg → bits 6–8, col-reg → bits 2–4, ARE in 0–1.
 */
static unsigned int encode_matrix_regs(const char *regstr) {
    if (!regstr) return FALSE;

    const char *p = regstr;
    int r_row = -1, r_col = -1;

    while (*p && isspace((unsigned char)*p)) p++;

    if (*p != '[') return FALSE;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p != 'r') return FALSE;
    p++;

    char *endptr = NULL;
    long v = strtol(p, &endptr, 10);
    if (endptr == p || v < 0 || v > 7) return FALSE;
    r_row = (int)v;
    p = endptr;

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ']') return FALSE;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p != '[') return FALSE;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p != 'r') return FALSE;
    p++;

    v = strtol(p, &endptr, 10);
    if (endptr == p || v < 0 || v > 7) return FALSE;
    r_col = (int)v;
    p = endptr;

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ']') return FALSE;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p != '\0') return FALSE;  /* trailing junk not allowed */

    return ((r_row & 0x7) << 6) | ((r_col & 0x7) << 2) | A_ARE;
}

/*
 * encode_command_line
 * -------------------
 * Builds the FIRST word of an instruction line.
 */
int encode_command_line(Row *row, const char *src_filename)
{
    if (!row) return FALSE;

    int src_mode = 0, dest_mode = 0;
    char operands_copy[MAX_OPERAND_LEN];
    char *src_tok = NULL, *dest_tok = NULL;

    strncpy(operands_copy, row->operands_string ? row->operands_string : "", MAX_OPERAND_LEN - 1);
    operands_copy[MAX_OPERAND_LEN - 1] = '\0';

    src_tok  = strtok(operands_copy, COMMA_STRING);
    dest_tok = strtok(NULL,          COMMA_STRING);

    if (src_tok)  src_mode  = detect_mode(src_tok);
    if (dest_tok) dest_mode = detect_mode(dest_tok);

    const unsigned src_all   = 0xF;    /* 0|1|2|3 */
    const unsigned dest_all  = 0xF;
    const unsigned not_immediate = 0xE;/* 1|2|3 */
    const unsigned lea_src   = 0x6;    /* 1|2 only */

    int cmd = row->command;
    int expected_operands = 2;
    unsigned allow_src = 0, allow_dst = 0;

    switch (cmd) {
        case MOV: allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;
        case CMP: allow_src = src_all;        allow_dst = dest_all;      expected_operands = 2; break;
        case ADD: allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;
        case SUB: allow_src = src_all;        allow_dst = not_immediate; expected_operands = 2; break;
        case NOT: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case CLR: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case LEA: allow_src = lea_src;        allow_dst = not_immediate; expected_operands = 2; break;
        case INC: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case DEC: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case JMP: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case BNE: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case RED: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case PRN: allow_src = 0;              allow_dst = dest_all;      expected_operands = 1; break;
        case JSR: allow_src = 0;              allow_dst = not_immediate; expected_operands = 1; break;
        case RTS: allow_src = 0;              allow_dst = 0;             expected_operands = 0; break;
        case STP: allow_src = 0;              allow_dst = 0;             expected_operands = 0; break;
        default:
            error_at_row(src_filename, row, "Unknown command opcode");
            return FALSE;
    }

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
        if (src_tok && !dest_tok) {
            dest_tok  = src_tok;
            dest_mode = src_mode;
            src_tok   = NULL;
            src_mode  = 0;
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
    } else {
        if (provided_operands != 0) {
            print_error(src_filename, (int)row->original_line_number, "Expected no operands for this instruction");
            return FALSE;
        }
    }

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

    unsigned int binary = 0;
    binary |= (row->command & 0xF) << 6;
    binary |= (src_tok  ? (src_mode  & 0x3) << 4 : 0);
    binary |= (dest_tok ? (dest_mode & 0x3) << 2 : 0);
    binary |= A_ARE;

    row->binary_machine_code = (binary & TEN_BIT_MASK);
    return TRUE;
}

int encode_two_register_operands(const char *operand, Row *row) {
    if (!operand) return FALSE;

    char buf[MAX_OPERAND_LEN + 1];
    int i = 0;
    while (operand[i] != '\0' && i < MAX_OPERAND_LEN) {
        buf[i] = operand[i];
        i++;
    }
    buf[i] = '\0';

    char *comma = strchr(buf, ',');
    if (!comma) return FALSE;

    *comma = '\0';
    char *left  = buf;
    char *right = comma + 1;

    while (*left == ' ' || *left == '\t') left++;
    char *end = left + strlen(left) - 1;
    while (end >= left && (*end == ' ' || *end == '\t')) { *end-- = '\0'; }

    while (*right == ' ' || *right == '\t') right++;
    end = right + strlen(right) - 1;
    while (end >= right && (*end == ' ' || *end == '\t')) { *end-- = '\0'; }

    if (*right == '\0' || *left == '\0') return FALSE;

    int r1 = -1, r2 = -1;
    if (sscanf(left, "r%d", &r1) != 1) return FALSE;
    if (sscanf(right, "r%d", &r2) != 1) return FALSE;

    if (r1 < 0 || r1 > 7 || r2 < 0 || r2 > 7) return NOT_FOUND;

    unsigned int word = ((r1 & 0xF) << 6) | ((r2 & 0xF) << 2) | A_ARE;
    row->binary_machine_code = (word & TEN_BIT_MASK);

    return TRUE;
}

int encode_operand_row(Row *row, Labels *labels, const char *src_filename)
{
    if (!row) return FALSE;

    const char *operand = row->operands_string;
    if (!operand || !*operand) {
        error_at_row(src_filename, row, "Missing operand text for encoding");
        return FALSE;
    }

    if (is_matrix(operand) == FALSE_FOUND) {
        error_at_row(src_filename, row, "Only registers between r0 and r7 are allowed");
        return FALSE;
    }

    int two_register_operands = encode_two_register_operands(operand, row);
    if (two_register_operands == TRUE) return TRUE;
    if (two_register_operands == NOT_FOUND) {
        print_error(src_filename, (int)row->original_line_number,
                     "Invalid two-register operand syntax; expected 'rX, rY' with X and Y in 0..7");
        return FALSE;
    }

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

    if (is_immediate(operand)) {
        double val = 0.0;
        if (!is_number(operand + 1, &val)) {
            print_error(src_filename, (int)row->original_line_number,"Invalid immediate operand; expected # followed by a number");
            return FALSE;
        }
        unsigned int payload = (unsigned int)((int)val & 0xFF);
        row->binary_machine_code = pack_payload_with_are(payload, A_ARE) & TEN_BIT_MASK;
        return TRUE;
    }

    if (is_register(operand)) {
        int regnum = atoi(&operand[1]);
        if (regnum < 0 || regnum > 7) {
            error_at_row(src_filename, row, "Register index out of range (expected r0..r7)");
            return FALSE;
        }
        unsigned int role_hint = row->binary_machine_code; /* 1=src, 2=dest */
        unsigned int word = 0;

        if (role_hint == 1) {          /* source */
            word = ((unsigned int)regnum << 6) | A_ARE;
        } else if (role_hint == 2) {   /* dest */
            word = ((unsigned int)regnum << 2) | A_ARE;
        } else {
            error_at_row(src_filename, row, "Missing role hint for single-register operand");
            return FALSE;
        }

        row->binary_machine_code = (word & TEN_BIT_MASK);
        return TRUE;
    }
    if (is_register(operand) == NOT_FOUND) {
        error_at_row(src_filename, row, "Invalid operand: expected a register (r0..r7)");
        return FALSE;
    }

    int two_operands = encode_two_register_operands(operand, row);
    if (two_operands == TRUE) return TRUE;
    if (two_operands == NOT_FOUND) {
        error_at_row(src_filename, row, "Invalid two-register, register must be r0 to r7");
        return FALSE;
    }

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
            row->binary_machine_code = 0;
            return TRUE;
        }
        row->binary_machine_code = ((unsigned char)row->operands_string[0]) & TEN_BIT_MASK;
        return TRUE;
    }

    if (row->command == DAT || row->command == MAT) {
        double val;
        if (is_number(row->operands_string, &val)) {
            long iv = (long)val;
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
            if (encode_command_line(row, src_filename) == FALSE) {
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
                had_error = TRUE;
            }
        }
    }

    return had_error ? FALSE : TRUE;
}
