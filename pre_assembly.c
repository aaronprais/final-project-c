#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "logger.h"

#define MAX_LINE_LENGTH 82
#define MAX_MACROS 100
#define MAX_MACRO_LINES 50
#define MAX_MACRO_NAME 31
#define MAX_FILENAME 100
#define MAX_LABEL_LENGTH 31

typedef struct
{
    char name[MAX_MACRO_NAME];
    char lines[MAX_MACRO_LINES][MAX_LINE_LENGTH];
    int line_count;
} Macro;

Macro macro_table[MAX_MACROS];
int macro_count = 0;

int report_error_and_advance(const char *message, int line_number, int *has_errors)
{
    printf("Line %d: Error – %s\n", line_number, message);
    (*has_errors)++;
    return line_number + 1;
}

void normalize_and_validate_line(char *line, int line_number, int *has_errors)
{
    char cleaned[MAX_LINE_LENGTH];
    int len = strlen(line);

    while (len > 0 && isspace(line[len - 1]))
    {
        line[--len] = '\0';
    }

    int start = 0;
    while (isspace(line[start]))
    {
        start++;
    }

    if (start > 0)
    {
        memmove(line, line + start, strlen(line + start) + 1);
    }

    int i = 0, j = 0;
    int in_word = 0;

    while (line[i])
        {
        if (isspace(line[i]))
        {
            if (in_word)
            {
                cleaned[j++] = ' ';
                in_word = 0;
            }
        }
        else
        {
            if (j > 0 && !isspace(cleaned[j - 1]) && !in_word && isalnum(line[i]) && isalnum(cleaned[j - 1]))
            {
                char msg[100];
                snprintf(msg, sizeof(msg), "missing space between words near '%c%c'", cleaned[j - 1], line[i]);
                report_error_and_advance(msg, line_number, has_errors);
            }
            cleaned[j++] = line[i];
            in_word = 1;
        }
        i++;
    }

    if (j > 0 && cleaned[j - 1] == ' ')
        j--;
    cleaned[j] = '\0';
    strcpy(line, cleaned);
}

int is_empty_or_comment(const char *line)
{
    while (*line && isspace(*line))
    {
        line++;
    }

    if (*line == '\0')
    {
        return 1;
    }

    if (*line == ';')
    {
        return 1;
    }
    return 0;
}

void run_pre_assembler(FILE *in, FILE *out)
{
    char line[MAX_LINE_LENGTH];
    int line_number = 1;
    int has_errors = 0;

    while (fgets(line, MAX_LINE_LENGTH, in))
    {
        if (strlen(line) > 80)
        {
            line_number = report_error_and_advance("line too long (max 80 chars)", line_number, &has_errors);
            continue;
        }

        normalize_and_validate_line(line, line_number, &has_errors);

        if (is_empty_or_comment(line))
        {
            fprintf(out, "%s\n", line);
            line_number++;
            continue;
        }

        if (starts_with_macro(line))
        {
            if (!handle_macro_definition(in, line))
            {
                printf("Line %d: Error in macro definition\n", line_number);
                has_errors++;
            }
            line_number++;
            continue;
        }

        if (is_macro_name(line))
        {
            if (!expand_macro(line, out))
            {
                printf("Line %d: Error expanding macro\n", line_number);
                has_errors++;
            }
        }
        else
        {
            if (!validate_line(line, line_number))
            {
                has_errors++;
            }
            else
            {
                fprintf(out, "%s\n", line);
            }
        }
        line_number++;
    }

    if (has_errors)
    {
        printf("\n❌ %d errors were found. Assembly process aborted.\n", has_errors);
    }
}







