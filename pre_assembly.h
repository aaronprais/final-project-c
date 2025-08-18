#ifndef PRE_ASSEMBLY_H
#define PRE_ASSEMBLY_H

#include <stdio.h>
#include "util.h"

/* macro object - kinda simple: name + captured lines */
typedef struct {
    char name[MAX_LINE_LENGTH];
    char **lines;
    int line_count;
    int capacity;
} Macro;

/* table of macros (encapsulated state, no globals here) */
typedef struct {
    Macro *data;
    int count;
    int capacity;
} MacroTable;

/* lifecycle (freeing mem is imporant lol) */
void free_macro_table(MacroTable *mtbl);

/* main api (preprocess runs on a FILE*, runner makes .am file) */
int preprocess_file(FILE *in, FILE *out, const char *filename, MacroTable *mtbl);
int run_pre_assembly(FILE *in, const char *base_filename);

#endif /* PRE_ASSEMBLY_H */
