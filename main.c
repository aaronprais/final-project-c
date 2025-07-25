#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "labels.h"
#include "ordering_into_table.h"
#include "table.h"

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

        // int count = count_labels(fp);  // call again to get label count
        // rewind(fp);  // needed if file was closed before
        //
        // char **labels = parse_labels_from_file(fp);
        // if (!labels) {
        //     printf("Label parsing failed.\n");
        //     return 1;
        // }
        //
        // printf("Labels found:\n");
        // int j;
        // for (j = 0; j < count; j++) {
        //     printf(" - %s\n", labels[j]);
        //     free(labels[j]);
        // }
        // free(labels);


        fclose(fp);
    }

    return EXIT_SUCCESS;
}