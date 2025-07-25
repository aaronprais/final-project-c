#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pre_assembly.h"
#include "util.h"

Macro *macro_table = NULL;
int macro_count = 0;
int macro_capacity = 0;

// Allocates memory safely with error check
static void *checked_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    return ptr;
}
// Reallocates memory safely with error check
static void *checked_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr)
    {
        fprintf(stderr, "Memory reallocation failed\n");
        exit(1);
    }
    return new_ptr;
}
// Adds a macro to the macro table (deep copy)
static void add_macro(Macro macro)
{
    if (macro_count >= macro_capacity)
    {
        macro_capacity = (macro_capacity == 0) ? DEFAULT_MACRO_CAPACITY : macro_capacity * GROWTH_FACTOR;
        macro_table = checked_realloc(macro_table, macro_capacity * sizeof(Macro));
    }

    Macro copy;
    strcpy(copy.name, macro.name);
    copy.line_count = macro.line_count;
    copy.capacity = macro.capacity;

    copy.lines = checked_malloc(copy.capacity * sizeof(char *));
    int i;
    for (i = 0; i < copy.line_count; i++)
    {
        copy.lines[i] = checked_malloc(strlen(macro.lines[i]) + 1);
        strcpy(copy.lines[i], macro.lines[i]);
    }

    macro_table[macro_count++] = copy;
}

// Trims leading and trailing whitespace/newlines from a line
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

// Checks if the line starts a macro definition
static int is_macro_start(const char *line)
{
    return strncmp(line, MACRO_KEYWORD, MACRO_LEN) == 0 && (line[MACRO_LEN] == NULL_CHAR || isspace(line[MACRO_LEN]));
}

// Checks if the line ends a macro definition
static int is_macro_end(const char *line)
{
    return strncmp(line, MACRO_END_KEYWORD, MACRO_END_LEN) == 0 && (line[MACRO_END_LEN] == NULL_CHAR || isspace(line[MACRO_END_LEN]));
}

// Duplicates a string with safe allocation
static char *safe_strdup(const char *s)
{
    char *copy = malloc(strlen(s) + 1);
    if (!copy)
    {
        fprintf(stderr, "Memory allocation failed while duplicating string\n");
        exit(1);
    }
    strcpy(copy, s);
    return copy;
}
// Adds a line to the current macro definition
static int add_line_to_macro(Macro *macro, const char *line)
{
    if (macro->line_count >= macro->capacity)
    {
        int new_capacity = (macro->capacity == 0) ? INITIAL_LINE_CAPACITY : macro->capacity * GROWTH_FACTOR;
        char **new_lines = realloc(macro->lines, new_capacity * sizeof(char *));
        if (!new_lines)
        {
            fprintf(stderr, "Memory allocation failed while expanding macro lines\n");
            return TRUE;
        }
        macro->lines = new_lines;
        macro->capacity = new_capacity;
    }

    macro->lines[macro->line_count++] = safe_strdup(line);
    return FALSE;
}

// Expands macro if the line matches a macro name
static int expand_macro_if_match(FILE *out, const char *line)
{
    char macro_candidate[MAX_LINE_LENGTH];
    char extra[MAX_LINE_LENGTH];
    int i, j;
    int matched = sscanf(line, "%s %s", macro_candidate, extra);

    for (i = 0; i < macro_count; ++i)
    {
        if (strcmp(macro_candidate, macro_table[i].name) == 0)
        {
            if (matched == 2)
            {
                printf("Error: unexpected text after macro invocation '%s'\n", macro_candidate);
                return TRUE;
            }
            for (j = 0; j < macro_table[i].line_count; ++j)
                fprintf(out, "%s\n", macro_table[i].lines[j]);

            return TRUE;
        }
    }
    return FALSE;
}

// Writes a normal line to output file, ensuring newline
static void write_normal_line(FILE *out, const char *line)
{
    fprintf(out, "%s", line);
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != NEWLINE_CHAR)
        fprintf(out, "%c", NEWLINE_CHAR);
}

// Checks if the name is a reserved command or directive
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
// Checks if the macro name was already defined
static int is_macro_already_defined(const char *name)
{
    int i;
    for (i = 0; i < macro_count; i++)
    {
        if (strcmp(name, macro_table[i].name) == 0)
            return TRUE;
    }
    return FALSE;
}

// Validates and initializes a macro definition line
static void handle_macro_definition(char *line, int line_number, int *inside_macro, int *inside_an_invalid_macro, int *had_error, Macro *current_macro)
{
    char name[MAX_LINE_LENGTH];
    char extra[MAX_LINE_LENGTH];
    int items_read;

    *inside_macro = FALSE;
    *inside_an_invalid_macro = TRUE;

    if (strlen(line) <= MACRO_LEN)
    {
        printf("Line %d: Error – missing macro name\n", line_number);
        *had_error = TRUE;
        return;
    }

    items_read = sscanf(line + MACRO_LEN + 1, "%s %s", name, extra);

    if (strlen(name) >= MAX_LINE_LENGTH)
    {
        printf("Line %d: Error – macro name too long (max %d chars)\n", line_number, MAX_LINE_LENGTH - 1);
        *had_error = TRUE;
        return;
    }

    if (items_read == 2)
    {
        printf("Line %d: Error – text after macro name is not allowed\n", line_number);
        *had_error = TRUE;
        return;
    }

    if (is_macro_already_defined(name))
    {
        printf("Line %d: Error – macro '%s' already defined\n", line_number, name);
        *had_error = TRUE;
        return;
    }

    if (is_reserved_macro_name(name))
    {
        printf("Line %d: Error – macro name '%s' is a reserved word\n", line_number, name);
        *had_error = TRUE;
        return;
    }

    strcpy(current_macro->name, name);
    current_macro->capacity = 10;
    current_macro->line_count = 0;
    current_macro->lines = malloc(current_macro->capacity * sizeof(char *));
    if (!current_macro->lines)
    {
        fprintf(stderr, "Memory allocation failed for macro lines\n");
        *had_error = TRUE;
        return;
    }

    *inside_macro = TRUE;
    *inside_an_invalid_macro = FALSE;
}

// Checks if a line exceeds allowed length
static int is_line_too_long(const char *line)
{
    return strlen(line) == MAX_LINE_LENGTH - 1 && line[MAX_LINE_LENGTH - 2] != NEWLINE_CHAR;
}
// Skips input lines until 'mcroend' is found
static void skip_until_macro_end(FILE *in, int *line_number)
{
    char temp[MAX_LINE_LENGTH];
    while (fgets(temp, MAX_LINE_LENGTH, in))
    {
        (*line_number)++;
        if (is_macro_end(temp)) break;
    }
}
// Checks if extra text exists after 'mcroend'
static int has_text_after_macroend(const char *line)
{
    const char *after = line + strlen(MACRO_END_KEYWORD);
    while (isspace((unsigned char)*after)) after++;
    return *after != NULL_CHAR;
}

void free_macro_table()
{
    int i, j;
    for (i = 0; i < macro_count; i++)
    {
        for (j = 0; j < macro_table[i].line_count; j++)
            free(macro_table[i].lines[j]);

        free(macro_table[i].lines);
    }
    free(macro_table);
    macro_table = NULL;
    macro_count = 0;
    macro_capacity = 0;
}
// Main preprocessing function for handling macro expansion
int preprocess_file(FILE *in, FILE *out)
{
    int had_error = FALSE;
    char line[MAX_LINE_LENGTH];
    int inside_macro = FALSE;
    int inside_an_invalid_macro = FALSE;
    Macro current_macro;
    int line_number = ONE;

    while (fgets(line, MAX_LINE_LENGTH, in))
    {
        if (is_line_too_long(line))
        {
            printf("Line %d: Error – line exceeds 80 characters\n", line_number);
            had_error = TRUE;
            inside_an_invalid_macro = TRUE;
            int ch;
            while ((ch = fgetc(in)) != NEWLINE_CHAR && ch != EOF);
            line_number++;
            continue;
        }

        trim_line(line);

        if (inside_macro && is_macro_start(line))
        {
            printf("Line %d: Error – cannot define a macro inside another macro\n", line_number);
            had_error = TRUE;
            skip_until_macro_end(in, &line_number);
            inside_macro = FALSE;
            continue;
        }

        if (!inside_macro && !inside_an_invalid_macro && is_macro_end(line))
        {
            printf("Line %d: Error – 'mcroend' without matching 'mcro'\n", line_number);
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
                    printf("Line %d: Error – text after 'mcroend' is not allowed\n", line_number);
                    had_error = TRUE;
                }
                add_macro(current_macro);
                inside_macro = FALSE;
                inside_an_invalid_macro = FALSE;
            }
            else
            {
                if (add_line_to_macro(&current_macro, line))
                    had_error = TRUE;
            }
            line_number++;
            continue;
        }

        if (is_macro_start(line))
        {
            handle_macro_definition(line, line_number, &inside_macro, &inside_an_invalid_macro, &had_error, &current_macro);
            line_number++;
            continue;
        }

        if (expand_macro_if_match(out, line))
        {
            line_number++;
            continue;
        }
        write_normal_line(out, line);
        line_number++;
    }
    free_macro_table();
    return had_error;
}

int run_pre_assembly(FILE *in, const char *base_filename)
{
    char output_filename[MAX_FILENAME];
    snprintf(output_filename, MAX_FILENAME, "%s.am", base_filename);

    FILE *out = fopen(output_filename, "w");
    if (!out)
    {
        fprintf(stderr, "Error - cannot create output file: %s\n", output_filename);
        return 1;
    }

    printf("Preprocessing file: %s.as\n", base_filename);

    int had_error = preprocess_file(in, out);

    if (had_error)
    {
        remove(output_filename);
        printf("File '%s' was not created due to errors.\n", output_filename);
    }
    else
    {
        printf("File '%s' created successfully.\n", output_filename);
    }

    fclose(out);
    free_macro_table();

    return had_error;
}

