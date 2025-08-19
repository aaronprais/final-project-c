#include "file_formating.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "labels.h"
#include "util.h"

/* ----------- Base-4 encoding helpers ----------- */

/* digit_to_char
 * -------------
 * Converts a base-4 digit to a char (0→'a', 1→'b', 2→'c', 3→'d').
 */
static char digit_to_char(int d) {
    if (d == 0) return 'a';
    if (d == 1) return 'b';
    if (d == 2) return 'c';
    if (d == 3) return 'd';
    return 'a'; /* fallback if invalid, should not happen */
}

/* to_base4_address
 * ----------------
 * Turns an unsigned int address into 4 base-4 letters.
 * Out buffer must be at least 5 chars (last is null terminator).
 */
static void to_base4_address(unsigned int addr, char out[5]) {
    int i;
for (i = 3; i >= 0; --i) {
        int digit = addr % 4;
        out[i] = digit_to_char(digit);
        addr /= 4;
    }
    out[4] = '\0';
}

/* to_base4_code
 * -------------
 * Turns binary machine code into 5 base-4 letters.
 * Out buffer must be at least 6 chars (last is null terminator).
 */
static void to_base4_code(unsigned int code, char out[6]) {
    int i;
for (i = 4; i >= 0; --i) {
        int digit = code % 4;
        out[i] = digit_to_char(digit);
        code /= 4;
    }
    out[5] = '\0';
}

/* ----------- Export functions ----------- */

/*
 * export_object_file
 * ------------------
 * Writes each row of the table to <name>.ob
 * Format: <addr in base-4> \t <code in base-4>
 */
int export_object_file(Table *tbl, const char *name) {
    if (!tbl || !name) {
        fprintf(stderr, "%s: Error - no data found for .ob file\n", name);
        return FALSE;
    }

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ob", name);

    FILE *fp = fopen(filename, "w");
    if (!fp){
        fprintf(stderr, "%s: Error - failed to write .ob file\n", name);
        return FALSE;
    }

    int written_into_file = FALSE;

    int i;
    for (i = 0; i < tbl->size; ++i) {
        char addr_base4[5];
        char code_base4[6];
        to_base4_address(tbl->data[i].decimal_address, addr_base4);
        to_base4_code(tbl->data[i].binary_machine_code, code_base4);

        fprintf(fp, "%s\t%s\n", addr_base4, code_base4);
        written_into_file = TRUE;
    }

    fclose(fp);
    if (!written_into_file) {
        remove(filename);
        return FALSE;
    }
    return TRUE;
}

/*
 * export_entry_file
 * -----------------
 * Writes all labels marked as .entry into <name>.ent
 * It searches for same label in non-entry context to grab its adress.
 */
int export_entry_file(Labels *lbls, const char *name) {
    if (!lbls || !name){
        fprintf(stderr, "%s: Error - no data found for .ex file\n", name);
        return FALSE;
    }

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ent", name);

    FILE *fp = fopen(filename, "w");
    if (!fp){
        fprintf(stderr, "%s: Error - failed to write .ent file\n", name);
        return FALSE;
    }

    int written_into_file = FALSE;

    int i;
    for (i = 0; i < lbls->size; i++) {
        if (lbls->data[i].is_entry) {
            char *current_label = lbls->data[i].label;

            int j;
            for (j = 0; j < lbls->size; j++) {
                if (strcmp(lbls->data[j].label, current_label) == 0 &&
                    !(lbls->data[j].is_entry)) {
                    char addr_base4[5];
                    to_base4_address(lbls->data[j].decimal_address, addr_base4);
                    fprintf(fp, "%s\t%s\n", current_label, addr_base4);
                    written_into_file = TRUE;
                }
            }
        }
    }

    fclose(fp);
    if (!written_into_file) {
        remove(filename);
        return FALSE;
    }
    return TRUE;
}

/*
 * export_external_file
 * --------------------
 * Writes all occurences of .extern labels into <name>.ext
 * It scans the table rows that are NOT command-lines (data/operands).
 */
int export_external_file(Table *tbl, Labels *lbls, const char *name) {
    if (!lbls || !name || !tbl){
        fprintf(stderr, "%s: Error - no data found for .ex file\n", name);
        return FALSE;
    }

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ext", name);

    FILE *fp = fopen(filename, "w");
    if (!fp){
        fprintf(stderr, "%s: Error - failed to write .ex file\n", name);
        return FALSE;
    }

    int written_into_file = FALSE;

    int i;
    for (i = 0; i < tbl->size; i++) {
        if (!tbl->data[i].is_command_line) {
            Label *lbl = find_label_by_name(lbls, tbl->data[i].operands_string);

            if (lbl && lbl->type == EXT) {
                char addr_base4[5];
                to_base4_address(tbl->data[i].decimal_address, addr_base4);
                fprintf(fp, "%s\t%s\n", lbl->label, addr_base4);
                written_into_file = TRUE;
            }
        }
    }

    fclose(fp);
    if (!written_into_file) {
        remove(filename);
        return FALSE;
    }
    return TRUE;
}
