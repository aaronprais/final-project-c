#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "labels.h"
#include "ordering_into_table.h"
#include "table.h"
#include "binary_table_parsing.h"
#include "pre_assembly.h"
#include "file_formating.h"

int main(int argc, char *argv[]) {
    FILE *fp;
    char filename[MAX_FILENAME];

    /* CLI usage check — must pass at least one base file name (without ext) */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int i;

    /* iterate user-supplied input basenames: foo → foo.as → foo.am → outputs */
    for (i = 1; i < argc; i++) {

        /* ---------- Stage 1: pre-assembly on <file>.as ---------- */
        /* expands macros etc; outputs a .am file on success */
        snprintf(filename, MAX_FILENAME, "%s.as", argv[i]);     /* build source path */
        fp = fopen(filename, "r");
        if (fp == NULL) {
            /* cant open input file — probably bad path or perms */
            fprintf(stderr, "%s: Error - cannot open .as file\n", argv[i]);
            fprintf(stderr,
                    "%s: Due to errors no | .ob | .ext | .ent | files created\n",
                    argv[i]);
            continue; /* process next file (dont crash whole batch) */
        }

        int failed = run_pre_assembly(fp, argv[i]); /* argv[i] is base name w/o ext */
        fclose(fp);
        if (failed) {
            /* pre-assembly reported an error; we skip later stages safely */
            fprintf(stderr,
                    "%s: Due to errors no | .ob | .ext | .ent | files created\n",
                    argv[i]);
            continue;
        }

        /* ---------- Stage 2: build table & labels from <file>.am ---------- */
        /* parses tokens, fills Table + Labels; performs semantic checks (kinda strict) */
        snprintf(filename, MAX_FILENAME, "%s.am", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "%s: Error - cannot open .am file\n", argv[i]);
            fprintf(stderr,
                    "%s: Due to errors no | .ob | .ext | .ent | files created\n",
                    argv[i]);
            continue; /* process next file */
        }

        Table *tbl = create_table();            /* holds rows (IC/DC stuff) */
        Labels *lbls = create_label_table();    /* symbol table (entries, externs, etc) */

        if (!tbl || !lbls) {
            fprintf(stderr, "Error: failed to allocate memory for table or labels\n");
            fclose(fp);
            if (tbl) free_table(tbl);
            if (lbls) free_label_table(lbls);
            continue; /* process next file */
        }

        failed = process_file_to_table_and_labels(tbl, lbls, fp, filename);
        fclose(fp);
        if (failed) {
            /* parsing error — free memory and bail out for this file */
            free_table(tbl);
            free_label_table(lbls);
            fprintf(stderr,
                    "%s: Due to errors no | .ob | .ext | .ent | files created\n",
                    argv[i]);
            continue;
        }

        /* Set IC/DC base addresses (offset 100) consistently on both tables
           (this keeps machine code addresses aligned to the spec’s base adress). */
        reset_addresses(tbl, 100);
        reset_labels_addresses(lbls, 100);

        /* ---------- Stage 3: translate table → binary using labels ---------- */
        /* resolves symbols and outputs the internal binary representation (kinda cool) */
        if (!parse_table_to_binary(tbl, lbls, filename)) {
            fprintf(stderr,
                    "%s: Due to errors no | .ob | .ext | .ent | files created\n",
                    argv[i]);
            free_table(tbl);
            free_label_table(lbls);
            continue;
        }

        /* ---------- Export artifacts (.ob / .ent / .ext) ---------- */
        /* object file (final opcodes + data) */
        if (!export_object_file(tbl, argv[i])) {
            fprintf(stdout,"%s: .ob file not created\n", argv[i]);
        }
        else {
            fprintf(stdout, "%s: .ob file created\n", argv[i]);
        }

        /* entry file (symbols marked as .entry) */
        if (!export_entry_file(lbls, argv[i])) {
            fprintf(stdout, "%s: .ent file not created\n", argv[i]);
        }
        else {
            fprintf(stdout, "%s: .ent file created\n", argv[i]);
        }

        /* external references file (for .extern usages) */
        if (!export_external_file(tbl, lbls, argv[i])) {
            fprintf(stdout, "%s: .ext file not created\n", argv[i]);
        }
        else {
            fprintf(stdout, "%s: .ext file created", argv[i]);
        }

        /* Cleanup per file (no globals, so leak-free yay) */
        free_table(tbl);
        free_label_table(lbls);

        /* lil success message (kinda verbose but nice for users) */
        fprintf(stdout,"%s: Successfully compiled\n", argv[i]);
    }

    return EXIT_SUCCESS;
}
