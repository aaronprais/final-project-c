#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "labels.h"
#include "ordering_into_table.h"
#include "table.h"
#include "binary_table_parsing.h"
#include "pre_assembly.h"
#include "file_formating.h"

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
            fprintf(stderr, "Due to error in %s, no .am - .ob - .ent - .ext files created\n", filename);
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

        failed = process_file_to_table_and_labels(tbl, lbls, fp, filename);
        fclose(fp);
        if (failed) {
            /* pre-assembly reported an error for this file; skip to next */
            free_table(tbl);
            free_label_table(lbls);
            fprintf(stderr, "Due to error in %s, no .ob - .ext - .ent files created\n", filename);
            continue;
        }

        /* Set IC/DC base addresses (offset 100) consistently on both tables */
        reset_addresses(tbl, 100);
        reset_labels_addresses(lbls, 100);


        /* ---------- Stage 3: translate table → binary using labels ---------- */
        if (!parse_table_to_binary(tbl, lbls, filename)) {
            fprintf(stderr, "Due to error in %s, no .ob - .ext - .ent files created\n", filename);
            free_table(tbl);
            free_label_table(lbls);
            continue;
        }

        if (!export_object_file(tbl, argv[i])) {
            fprintf(stderr, "Due to error in %s, no .ob - .ext - .ent files created\n", filename);
            continue;
        }
        if (!export_entry_file(lbls, argv[i])){
            fprintf(stderr, "ERROR: %s, no .ent - .ext files created\n", filename);
            continue;
        }
        if (!export_external_file(tbl, lbls, argv[i])) {
            fprintf(stderr, "ERROR: %s, no .ext file created\n", filename);
            continue;
        }

        /* Cleanup per file (no globals) */
        free_table(tbl);
        free_label_table(lbls);

        fprintf(stdout, "Successfully compiled: %s.as and %s.ob, %s.ent, %s.ext files created\n", argv[i], argv[i], argv[i], argv[i]);
    }

    return EXIT_SUCCESS;
}