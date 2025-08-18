#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "labels.h"
#include "ordering_into_table.h"
#include "table.h"
#include "binary_table_parsing.h"
#include "pre_assembly.h"
#include "file_formating.h"

#define MAX_FILENAME 100  /* buffer for "<base>.ext" file names (should be enuf) */

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
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; /* process next file (dont crash whole batch) */
        }

        int failed = run_pre_assembly(fp, argv[i]); /* argv[i] is base name w/o ext */
        fclose(fp);
        if (failed) {
            /* pre-assembly reported an error; we skip later stages safely */
            fprintf(stderr,
                    "Due to error in %s, no .am - .ob - .ent - .ext files created\n",
                    filename);
            continue;
        }

        /* ---------- Stage 2: build table & labels from <file>.am ---------- */
        /* parses tokens, fills Table + Labels; performs semantic checks (kinda strict) */
        snprintf(filename, MAX_FILENAME, "%s.am", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; /* process next file */
        }

        Table *tbl = create_table();            /* holds rows (IC/DC stuff) */
        Labels *lbls = create_label_table();    /* symbol table (entries, externs, etc) */

        failed = process_file_to_table_and_labels(tbl, lbls, fp, filename);
        fclose(fp);
        if (failed) {
            /* parsing error — free memory and bail out for this file */
            free_table(tbl);
            free_label_table(lbls);
            fprintf(stderr,
                    "Due to error in %s, no .ob - .ext - .ent files created\n",
                    filename);
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
                    "Due to error in %s, no .ob - .ext - .ent files created\n",
                    filename);
            free_table(tbl);
            free_label_table(lbls);
            continue;
        }

        /* ---------- Export artifacts (.ob / .ent / .ext) ---------- */
        /* object file (final opcodes + data) */
        if (!export_object_file(tbl, argv[i])) {
            fprintf(stderr,
                    "Due to error in %s, no .ob - .ext - .ent files created\n",
                    filename);
            /* note: table/labels freed below if we keep going; here we just skip */
            continue;
        }

        /* entry file (symbols marked as .entry) */
        if (!export_entry_file(lbls, argv[i])) {
            fprintf(stderr, "ERROR: %s, no .ent - .ext files created\n", filename);
            continue;
        }

        /* external references file (for .extern usages) */
        if (!export_external_file(tbl, lbls, argv[i])) {
            fprintf(stderr, "ERROR: %s, no .ext file created\n", filename);
            continue;
        }

        /* Cleanup per file (no globals, so leak-free yay) */
        free_table(tbl);
        free_label_table(lbls);

        /* lil success message (kinda verbose but nice for users) */
        fprintf(stdout,
                "Successfully compiled: %s.as and %s.ob, %s.ent, %s.ext files created\n",
                argv[i], argv[i], argv[i], argv[i]);
    }

    return EXIT_SUCCESS;
}
