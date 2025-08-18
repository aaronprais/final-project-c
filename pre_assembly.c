#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pre_assembly.h"
#include "util.h"

/* ---------------- Safe allocation helpers (filename-aware) ---------------- */

static void *checked_malloc(const char *filename, size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        print_error(filename, 0, "Memory allocation failed");
        exit(1);
    }
    return ptr;
}

static void *checked_realloc(const char *filename, void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr)
    {
        print_error(filename, 0, "Memory reallocation failed");
        exit(1);
    }
    return new_ptr;
}

static char *safe_strdup(const char *filename, const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = checked_malloc(filename, n);
    strcpy(copy, s);
    return copy;
}

/* ---------------- Utility string helpers ---------------- */

static void trim_line(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == NEWLINE_CHAR))
        line[--len] = NULL_CHAR;

    char *start = line;
    while (isspace((unsigned char)*start))
        start++;

    if (start != line)
        memmove(line, start, strlen(start) + ONE);
}

static int is_macro_start(const char *line)
{
    return strncmp(line, MACRO_KEYWORD, MACRO_LEN) == 0 &&
           (line[MACRO_LEN] == NULL_CHAR || isspace((unsigned char)line[MACRO_LEN]));
}

static int is_macro_end(const char *line)
{
    return strncmp(line, MACRO_END_KEYWORD, MACRO_END_LEN) == 0 &&
           (line[MACRO_END_LEN] == NULL_CHAR || isspace((unsigned char)line[MACRO_END_LEN]));
}

static int is_line_too_long(const char *line)
{
    return strlen(line) == MAX_LINE_LENGTH - 1 && line[MAX_LINE_LENGTH - 2] != NEWLINE_CHAR;
}

static void skip_until_macro_end(FILE *in, int *line_number)
{
    char temp[MAX_LINE_LENGTH];
    while (fgets(temp, MAX_LINE_LENGTH, in))
    {
        (*line_number)++;
        if (is_macro_end(temp)) break;
    }
}

static int has_text_after_macroend(const char *line)
{
    const char *after = line + strlen(MACRO_END_KEYWORD);
    while (isspace((unsigned char)*after)) after++;
    return *after != NULL_CHAR;
}

/* ---------------- Macro table helpers (no globals) ---------------- */

static void add_macro(const char *filename, MacroTable *mtbl, const Macro *macro)
{
    if (mtbl->count >= mtbl->capacity)
    {
        mtbl->capacity = (mtbl->capacity == 0) ? DEFAULT_MACRO_CAPACITY : mtbl->capacity * GROWTH_FACTOR;
        mtbl->data = checked_realloc(filename, mtbl->data, mtbl->capacity * sizeof(Macro));
    }

    Macro copy;
    strcpy(copy.name, macro->name);
    copy.line_count = macro->line_count;
    copy.capacity = macro->capacity;

    copy.lines = checked_malloc(filename, copy.capacity * sizeof(char *));
    int i;

for (i = 0; i < copy.line_count; i++)
    {
        copy.lines[i] = safe_strdup(filename, macro->lines[i]);
    }

    mtbl->data[mtbl->count++] = copy;
}

static int is_reserved_macro_name(const char *name)
{
    int i;

for (i = 0; i < NUMBER_OF_COMMANDS + NUMBER_OF_DATA_TYPES; i++)
    {
        if (strcmp(name, command_names[i]) == 0)
            return TRUE;
    }
    return FALSE;
}

static int is_macro_already_defined(const MacroTable *mtbl, const char *name)
{
    int i;

for (i = 0; i < mtbl->count; i++)
    {
        if (strcmp(name, mtbl->data[i].name) == 0)
            return TRUE;
    }
    return FALSE;
}

void free_macro_table(MacroTable *mtbl)
{
    if (!mtbl) return;
    int i;

for (i = 0; i < mtbl->count; i++)
    {
        int j;

for (j = 0; j < mtbl->data[i].line_count; j++)
            free(mtbl->data[i].lines[j]);
        free(mtbl->data[i].lines);
    }
    free(mtbl->data);
    mtbl->data = NULL;
    mtbl->count = 0;
    mtbl->capacity = 0;
}

/* ---------------- Macro building helpers ---------------- */

static int add_line_to_macro(const char *filename, Macro *macro, const char *line)
{
    if (macro->line_count >= macro->capacity)
    {
        int new_capacity = (macro->capacity == 0) ? INITIAL_LINE_CAPACITY : macro->capacity * GROWTH_FACTOR;
        char **new_lines = realloc(macro->lines, new_capacity * sizeof(char *));
        if (!new_lines)
        {
            print_error(filename, -1, "Memory allocation failed while expanding macro lines");
            return TRUE;
        }
        macro->lines = new_lines;
        macro->capacity = new_capacity;
    }

    macro->lines[macro->line_count++] = safe_strdup(filename, line);
    return FALSE;
}

static void handle_macro_definition(const char *filename,
                                    char *line,
                                    int line_number,
                                    int *inside_macro,
                                    int *inside_an_invalid_macro,
                                    int *had_error,
                                    Macro *current_macro,
                                    MacroTable *mtbl)
{
    char name[MAX_LINE_LENGTH];
    char extra[MAX_LINE_LENGTH];
    int items_read;

    *inside_macro = FALSE;
    *inside_an_invalid_macro = TRUE;

    if (strlen(line) <= MACRO_LEN)
    {
        print_error(filename, line_number, "missing macro name");
        *had_error = TRUE;
        return;
    }

    items_read = sscanf(line + MACRO_LEN + 1, "%s %s", name, extra);

    if (strlen(name) >= MAX_LINE_LENGTH)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "macro name too long (max %d chars)", MAX_LINE_LENGTH - 1);
        print_error(filename, line_number, buf);
        *had_error = TRUE;
    }

    if (items_read == 2)
    {
        print_error(filename, line_number, "text after macro name is not allowed");
        *had_error = TRUE;
    }

    if (is_macro_already_defined(mtbl, name))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "macro '%s' already defined", name);
        print_error(filename, line_number, buf);
        *had_error = TRUE;
    }

    if (is_reserved_macro_name(name))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "macro name '%s' is a reserved word", name);
        print_error(filename, line_number, buf);
        *had_error = TRUE;
    }

    strcpy(current_macro->name, name);
    current_macro->capacity = 10;
    current_macro->line_count = 0;
    current_macro->lines = checked_malloc(filename, current_macro->capacity * sizeof(char *));
    if (!current_macro->lines)
    {
        print_error(filename, 0, "Memory allocation failed for macro lines");
        *had_error = TRUE;
        return;
    }

    *inside_macro = TRUE;
    *inside_an_invalid_macro = FALSE;
}

/* ---------------- Emission helpers ---------------- */

static int expand_macro_if_match(FILE *out,
                                 const char *filename,
                                 const char *line,
                                 int line_number,
                                 const MacroTable *mtbl,
                                 int *had_error)
{
    char first_token[MAX_LINE_LENGTH];

    if (sscanf(line, "%s", first_token) != 1)
        return FALSE;

    int i;

for (i = 0; i < mtbl->count; ++i)
    {
        if (strcmp(first_token, mtbl->data[i].name) == 0)
        {
            const char *p = line + strlen(first_token);
            while (isspace((unsigned char)*p)) p++;

            if (*p != '\0')
            {
                print_error(filename, line_number, "unexpected text after macro invocation");
                *had_error = TRUE;
            }

            int j;

for (j = 0; j < mtbl->data[i].line_count; ++j)
                fprintf(out, "%s\n", mtbl->data[i].lines[j]);

            return TRUE; /* expanded */
        }
    }

    return FALSE;
}

static void write_normal_line(FILE *out, const char *line)
{
    fprintf(out, "%s", line);
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != NEWLINE_CHAR)
        fprintf(out, "%c", NEWLINE_CHAR);
}

/* ---------------- Core: preprocess_file ---------------- */

int preprocess_file(FILE *in, FILE *out, const char *filename, MacroTable *mtbl)
{
    int had_error = FALSE;
    char line[MAX_LINE_LENGTH];
    int inside_macro = FALSE;
    int inside_an_invalid_macro = FALSE;
    Macro current_macro;
    int line_number = ONE;

    if (!mtbl) {
        print_error(filename, 0, "internal error: null MacroTable");
        return FALSE;
    }

    while (fgets(line, MAX_LINE_LENGTH, in))
    {
        if (is_line_too_long(line))
        {
            print_error(filename, line_number, "line exceeds 80 characters");
            had_error = TRUE;
            inside_an_invalid_macro = TRUE;
            int ch;
            while ((ch = fgetc(in)) != NEWLINE_CHAR && ch != EOF) { }
            line_number++;
            continue;
        }

        trim_line(line);

        if (inside_macro && is_macro_start(line))
        {
            print_error(filename, line_number, "cannot define a macro inside another macro");
            had_error = TRUE;
            skip_until_macro_end(in, &line_number);
            inside_macro = FALSE;
            continue;
        }

        if (!inside_macro && !inside_an_invalid_macro && is_macro_end(line))
        {
            print_error(filename, line_number, "'mcroend' without matching 'mcro'");
            had_error = TRUE;
            line_number++;
            continue;
        }

        if (inside_macro)
        {
            if (is_macro_end(line))
            {
                if (has_text_after_macroend(line))
                {
                    print_error(filename, line_number, "text after 'mcroend' is not allowed");
                    had_error = TRUE;
                }
                add_macro(filename, mtbl, &current_macro);
                inside_macro = FALSE;
                inside_an_invalid_macro = FALSE;
            }
            else
            {
                if (add_line_to_macro(filename, &current_macro, line))
                    had_error = TRUE;
            }
            line_number++;
            continue;
        }

        if (is_macro_start(line))
        {
            handle_macro_definition(filename, line, line_number,
                                    &inside_macro, &inside_an_invalid_macro, &had_error,
                                    &current_macro, mtbl);
            line_number++;
            continue;
        }

        if (expand_macro_if_match(out, filename, line, line_number, mtbl, &had_error))
        {
            line_number++;
            continue;
        }

        write_normal_line(out, line);
        line_number++;
    }

    return had_error;
}

/* ---------------- Top-level: run_pre_assembly ---------------- */

int run_pre_assembly(FILE *in, const char *base_filename)
{
    char output_filename[MAX_FILENAME];
    snprintf(output_filename, MAX_FILENAME, "%s.am", base_filename);

    FILE *out = fopen(output_filename, "w");
    if (!out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "cannot create output file: %s", output_filename);
        print_error(base_filename, 0, buf);
        return 1;
    }

    printf("Preprocessing file: %s.as\n", base_filename);

    MacroTable mtbl = {0};
    int had_error = preprocess_file(in, out, base_filename, &mtbl);

    if (had_error)
    {
        remove(output_filename);
    }
    else
    {
        printf("File '%s' created successfully.\n", output_filename);
    }

    fclose(out);
    free_macro_table(&mtbl);

    return had_error;
}
