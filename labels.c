#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "util.h"
#include "labels.h"

/* ---------------- Construction / Destruction ---------------- */

/* Allocates a new Labels table (initialy empty) */
Labels* create_label_table() {
    Labels *lbls = malloc(sizeof(Labels));
    if (!lbls) {
        print_error("SYSTEM", -1, "Failed to allocate labels table (malloc)");
        exit(EXIT_FAILURE); /* no point continuing if malloc fails */
    }
    lbls->data = NULL;
    lbls->size = 0;
    lbls->capacity = 0;
    return lbls;
}

/* Frees both the array data and the wrapper struct itself */
void free_label_table(Labels *lbls) {
    if (lbls) {
        free(lbls->data);
        free(lbls);
    }
}

/* Make sure we got enough room for new entries (realloc doubles size each time) */
void ensure_label_capacity(Labels *lbls) {
    if (lbls->size >= lbls->capacity) {
        lbls->capacity = (lbls->capacity == 0) ? 4 : lbls->capacity * 2;
        Label *new_data = realloc(lbls->data, lbls->capacity * sizeof(Label));
        if (!new_data) {
            print_error("SYSTEM", -1, "Failed to reallocate labels table (realloc)");
            exit(EXIT_FAILURE);
        }
        lbls->data = new_data;
    }
}

/* ---------------- Helpers ---------------- */

/* normalizes label string: trims whitespace + removes single trailing ':' */
static void normalize_label_name(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = NULL_CHAR;

    size_t len = strlen(start);
    if (len && start[len - 1] == SEMI_COLON_CHAR) {
        start[len - 1] = NULL_CHAR;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

/* ---------------- API ---------------- */

/* Adds a new label row (does validation of name and type) */
int add_label_row(Labels *lbls, const char *label, int table_row_index,
                  LabelTypes label_type, unsigned int is_entry,
                  int src_line, const char *src_filename) {
    char raw[MAX_LABEL_LEN + 1];
    int i = 0;

    if (is_register(label)) {
        print_error(src_filename, src_line, "Invalid label: cannot be a register name.");
        return FALSE;
    }

    /* copy string safely up to max length */
    while (label[i] != NULL_CHAR && i < MAX_LABEL_LEN) {
        raw[i] = label[i];
        i++;
    }
    raw[i] = NULL_CHAR;

    /* trim whitespace */
    char *start = raw;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + (int)strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = NULL_CHAR;

    int len = (int)strlen(start);
    if (len == 0) {
        print_error(src_filename, src_line, "Invalid label: empty after trimming.");
        return FALSE;
    }

    /* must start with a letter */
    if (!isalpha((unsigned char)start[0])) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Invalid label: \"%s\". Label must begin with an alphabetic character.", start);
        print_error(src_filename, src_line, msg);
        return FALSE;
    }

    /* rest must be alphanumeric */
    for (i = 0; i < len; i++) {
        if (!isalnum((unsigned char)start[i])) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Invalid label: \"%s\". Only letters and digits are allowed.", start);
            print_error(src_filename, src_line, msg);
            return FALSE;
        }
    }

    /* allocate space and copy into table */
    ensure_label_capacity(lbls);
    strncpy(lbls->data[lbls->size].label, start, MAX_LABEL_LEN);
    lbls->data[lbls->size].label[LABEL_NULL_CHAR_LOCATION] = NULL_CHAR;

    lbls->data[lbls->size].decimal_address = 0;
    lbls->data[lbls->size].table_row_index = table_row_index;
    lbls->data[lbls->size].type = label_type;
    lbls->data[lbls->size].is_entry = is_entry;

    lbls->size++;
    return TRUE;
}

/* safe index getter (returns NULL + error msg if out of bounds) */
Label* get_label(Labels *lbls, int index) {
    if (index < 0 || index >= lbls->size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Invalid label index %d", index);
        print_error("SYSTEM", -1, msg);
        return NULL;
    }
    return &lbls->data[index];
}

/* just checks if word ends with ':' (simple but works) */
int is_label(char *word) {
    size_t n = strlen(word);
    return (n > 0 && word[n - 1] == SEMI_COLON_CHAR) ? TRUE : FALSE;
}

/* resets all label addresses by adding offset (usualy 100) */
void reset_labels_addresses(Labels *lbls, unsigned int offset) {
    int i;
for (i = 0; i < lbls->size; i++) {
        lbls->data[i].decimal_address = offset + lbls->data[i].table_row_index;
    }
}

/* debug printer for labels table (nice for student testing) */
void print_labels(Labels *lbls) {
    printf("Row | Label           | Addr | Index | Type | Entry\n");
    printf("----+------------------+------+-----+----------\n");
    int i;
for (i = 0; i < lbls->size; i++) {
        printf("%-3d | %-15s | %4u | %-3d | %-3d | %-3d \n",
               i,
               lbls->data[i].label,
               lbls->data[i].decimal_address,
               lbls->data[i].table_row_index,
               lbls->data[i].type,
               lbls->data[i].is_entry);
    }
}

/* find a label by its name (ignores trailing ':' if user typed one) */
Label* find_label_by_name(const Labels *lbls, const char *name) {
    if (!lbls || !name) return NULL;

    char key[MAX_LABEL_LEN];
    strncpy(key, name, MAX_LABEL_LEN - 1);
    key[MAX_LABEL_LEN - 1] = NULL_CHAR;

    /* normalize */
    char *start = key;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = NULL_CHAR;
    size_t len = strlen(start);
    if (len && start[len - 1] == SEMI_COLON_CHAR) start[len - 1] = NULL_CHAR;

    int i;
for (i = 0; i < lbls->size; i++) {
        if (strcmp(lbls->data[i].label, start) == 0) {
            return (Label*)&lbls->data[i]; /* cast away const */
        }
    }
    return NULL;
}

/* count how many times a label name appears (could be duplicates in some cases) */
int count_label_by_name(const Labels *lbls, const char *name) {
    if (!lbls || !name) return 0;

    char key[MAX_LABEL_LEN];
    strncpy(key, name, MAX_LABEL_LEN - 1);
    key[MAX_LABEL_LEN - 1] = NULL_CHAR;

    char *start = key;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = NULL_CHAR;
    size_t len = strlen(start);
    if (len && start[len - 1] == SEMI_COLON_CHAR) start[len - 1] = NULL_CHAR;

    int count = 0;
    int i;
for (i = 0; i < lbls->size; i++) {
        if (strcmp(lbls->data[i].label, start) == 0) {
            count++;
        }
    }
    return count;
}
