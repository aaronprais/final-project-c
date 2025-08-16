#ifndef PRE_ASSEMBLY_H
#define PRE_ASSEMBLY_H

#include <stdio.h>
#include "util.h"

/* A collected macro: name + captured lines */
typedef struct
{
    char name[MAX_LINE_LENGTH];
    char **lines;
    int line_count;
    int capacity;
} Macro;

/* Encapsulated macro table state (no globals) */
typedef struct {
    Macro *data;
    int count;
    int capacity;
} MacroTable;

/* Lifecycle */
void free_macro_table(MacroTable *mtbl);

/* Main API */
int preprocess_file(FILE *in, FILE *out, const char *filename, MacroTable *mtbl);
int run_pre_assembly(FILE *in, const char *base_filename);

#endif /* PRE_ASSEMBLY_H */
