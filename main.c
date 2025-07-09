#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME 100

int main(int argc, char *argv[])
{
    FILE *fp;
    char filename[MAX_FILENAME];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int i;

    for (i = 1; i < argc; i++)
    {
        snprintf(filename, MAX_FILENAME, "%s.as", argv[i]);
        fp = fopen(filename, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "Error: cannot open %s\n", filename);
            continue; // process next file
        }

        printf("Processing file: %s\n", filename);
        fclose(fp);
    }

    return EXIT_SUCCESS;
}