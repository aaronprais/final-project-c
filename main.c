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
        snprintf(filename, MAX_FILENAME, "%s.as", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; // process next file
        }

        int failed = run_pre_assembly(fp, argv[i]);

        if (failed) {
            return EXIT_FAILURE;
        }

        snprintf(filename, MAX_FILENAME, "%s.am", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; // process next file
        }

        Table *tbl = create_table();
        Labels *lbls = create_label_table();


        process_file_to_table_and_labels(tbl, lbls, fp);
        reset_addresses(tbl, 100);
        reset_labels_addresses(lbls, 100);

        print_table(tbl);
        printf("------------------------------------------------------------------\n");
        print_labels(lbls);
        // printf("------------------------------------------------------------------\n");
        // parse_table_to_binary(tbl, lbls);
        // print_table(tbl);

        fclose(fp);
    }

    return EXIT_SUCCESS;
}