#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pre_assembly.h"
#include "util.h"

void free_macro_table()
{
    int j, k;
    for (j = 0; j < macro_count; j++)
    {
        for (k = 0; k < macro_table[j].line_count; k++)
        {
            free(macro_table[j].lines[k]);
        }
        free(macro_table[j].lines);
    }
    free(macro_table);
    macro_table = NULL;
    macro_count = 0;
    macro_capacity = 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file1> [file2] ...\n", argv[0]);
        return TRUE;
    }

    int i;
    for (i = 1; i < argc; i++)
    {
        size_t len = strlen(argv[i]);
        char *input_filename = malloc(len + strlen(".as") + 1);
        char *output_filename = malloc(len + strlen(".am") + 1);

        if (!input_filename || !output_filename)
        {
            fprintf(stderr, "Memory allocation failed.\n");
            free(input_filename);
            free(output_filename);
            continue;
        }

        snprintf(input_filename, len + strlen(".as") + 1, "%s.as", argv[i]);
        snprintf(output_filename, len + strlen(".am") + 1, "%s.am", argv[i]);

        FILE *in = fopen(input_filename, "r");
        if (!in)
        {
            fprintf(stderr, "Error - cannot open input file: %s\n", input_filename);
            free(input_filename);
            free(output_filename);
            continue;
        }

        FILE *out = fopen(output_filename, "w");
        if (!out)
        {
            fprintf(stderr, "Error - cannot create output file: %s\n", output_filename);
            fclose(in);
            free(input_filename);
            free(output_filename);
            continue;
        }

        printf("Preprocessing file: %s\n", argv[i]);
        int had_error = preprocess_file(in, out);

        if (had_error == TRUE)
        {
            remove(output_filename);
            printf("File '%s' was not created due to errors.\n", output_filename);
        }
        else
        {
            printf("File '%s' created successfully.\n", output_filename);
        }

        fclose(in);
        fclose(out);
        free(input_filename);
        free(output_filename);
        free_macro_table();
    }
    return FALSE;
}
