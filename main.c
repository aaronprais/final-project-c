#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "labels.h"
#include "ordering_into_table.h"
#include "table.h"
#include "binary_table_parsing.h"
#include "pre_assembly.h"

#define MAX_FILENAME 100

int main(int argc, char *argv[]) {
    FILE *fp;
    char filename[MAX_FILENAME];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int i;

    for (i = 1; i < argc; i++) {
        /* ---------- Stage 1: pre-assembly on <file>.as ---------- */
        snprintf(filename, MAX_FILENAME, "%s.as", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; /* process next file */
        }

        int failed = run_pre_assembly(fp, argv[i]);
        fclose(fp);
        if (failed) {
            /* pre-assembly reported an error for this file; skip to next */
            continue;
        }

        /* ---------- Stage 2: build table & labels from <file>.am ---------- */
        snprintf(filename, MAX_FILENAME, "%s.am", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; /* process next file */
        }

        Table *tbl = create_table();
        Labels *lbls = create_label_table();

        failed = process_file_to_table_and_labels(tbl, lbls, fp);
        fclose(fp);
        if (failed) {
            /* pre-assembly reported an error for this file; skip to next */
            continue;
        }

        /* Set IC/DC base addresses (offset 100) consistently on both tables */
        reset_addresses(tbl, 100);
        reset_labels_addresses(lbls, 100);

        /* Optional debug prints before encoding */
        printf("===== %s: TABLE BEFORE BINARY ENCODING =====\n", argv[i]);
        print_table(tbl);
        printf("------------------------------------------------------------------\n");
        print_labels(lbls);

        /* ---------- Stage 3: translate table → binary using labels ---------- */
        if (!parse_table_to_binary(tbl, lbls)) {
            fprintf(stderr, "Error: failed to translate table to binary for %s\n", argv[i]);
        } else {
            printf("------------------------------------------------------------------\n");
            printf("===== %s: TABLE AFTER BINARY ENCODING =====\n", argv[i]);
            print_table(tbl);
        }

        /* Cleanup per file (no globals) */
        free_table(tbl);
        free_label_table(lbls);
    }

    return EXIT_SUCCESS;
}