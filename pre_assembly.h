#ifndef PRE_ASSEMBLY_H
#define PRE_ASSEMBLY_H

#include "util.h"

typedef struct
{
    char name[MAX_LINE_LENGTH];
    char **lines;
    int line_count;
    int capacity;
} Macro;


extern Macro *macro_table;
extern int macro_count;
extern int macro_capacity;

int preprocess_file(FILE *in, FILE *out);

#endif
