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


void free_macro_table();
static void *checked_malloc(size_t size);
static void *checked_realloc(void *ptr, size_t size);
static void add_macro(Macro macro);
static void trim_line(char *line);
static int is_macro_start(const char *line);
static int is_macro_end(const char *line);
static char *safe_strdup(const char *s);
static int add_line_to_macro(Macro *macro, const char *line);
static int expand_macro_if_match(FILE *out, const char *line);
static void write_normal_line(FILE *out, const char *line);
static int is_reserved_macro_name(const char *name);
static int is_macro_already_defined(const char *name);
static void handle_macro_definition(char *line, int line_number, int *inside_macro, int *inside_an_invalid_macro, int *had_error, Macro *current_macro);
static int is_line_too_long(const char *line);
static void skip_until_macro_end(FILE *in, int *line_number);
static int has_text_after_macroend(const char *line);
int preprocess_file(FILE *in, FILE *out);
int run_pre_assembly(FILE *in, const char *base_filename);







#endif
