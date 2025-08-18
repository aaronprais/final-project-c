#include "file_formating.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "labels.h"
#include "util.h"

// convert base-4 digit to char
static char digit_to_char(int d) {
    if (d == 0) return 'a';
    if (d == 1) return 'b';
    if (d == 2) return 'c';
    if (d == 3) return 'd';
    return 'a'; // fallback
}

// helper to convert address to 4 base-4 chars
static void to_base4_address(unsigned int addr, char out[5]) {
    int i;
    for (i = 3; i >= 0; --i) {
        int digit = addr % 4;
        out[i] = digit_to_char(digit);
        addr /= 4;
    }
    out[4] = '\0';
}

// helper to convert binary machine code to 5 base-4 chars
static void to_base4_code(unsigned int code, char out[6]) {
    int i;
    for (i = 4; i >= 0; --i) {
        int digit = code % 4;
        out[i] = digit_to_char(digit);
        code /= 4;
    }
    out[5] = '\0';
}


// export the table to a filename.ob
int export_object_file(Table *tbl, const char *name) {
    if (!tbl || !name) return FALSE;

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ob", name);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return FALSE;
    }

    int i;
    for (i = 0; i < tbl->size; ++i) {
        char addr_base4[5];
        char code_base4[6];
        to_base4_address(tbl->data[i].decimal_address, addr_base4);
        to_base4_code(tbl->data[i].binary_machine_code, code_base4);

        fprintf(fp, "%s\t%s\n", addr_base4, code_base4);
    }

    fclose(fp);
    return TRUE;
}

int export_entry_file(Labels *lbls, const char *name) {
    if (!lbls || !name) return FALSE;

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ent", name);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return FALSE;
    }

    int i;
    for (i = 0; i < lbls->size; i++) {

        if (lbls->data[i].is_entry) {
            char *current_label = lbls->data[i].label;

            int j;
            for (j = i + 1; j < lbls->size; j++) {
                if (strcmp(lbls->data[j].label, current_label) == 0) {
                    char addr_base4[5];
                    to_base4_address(lbls->data[j].decimal_address, addr_base4);

                    fprintf(fp, "%s\t%s\n", current_label, addr_base4);
                }
            }
        }
    }

    fclose(fp);
    return TRUE;
}

int export_external_file(Table *tbl, Labels *lbls, const char *name) {
    if (!lbls || !name || !tbl) return FALSE;

    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s.ext", name);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return FALSE;
    }

    int i;
    for (i = 0; i < tbl->size; i++) {

        if (!tbl->data[i].is_command_line) {
            Label *lbl = find_label_by_name(lbls, tbl->data[i].operands_string);

            if (lbl && lbl->type == EXT) {
                char addr_base4[5];
                to_base4_address(tbl->data[i].decimal_address, addr_base4);

                fprintf(fp, "%s\t%s\n", lbl->label, addr_base4);
            }
        }
    }

    fclose(fp);
    return TRUE;
}