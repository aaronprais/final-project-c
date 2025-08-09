#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "labels.h"

/* ---------------- Construction / Destruction ---------------- */

Labels* create_label_table() {
    Labels *lbls = malloc(sizeof(Labels));
    if (!lbls) {
        perror("Failed to allocate labels table");
        exit(EXIT_FAILURE);
    }
    lbls->data = NULL;
    lbls->size = ZERO;
    lbls->capacity = ZERO;
    return lbls;
}

void free_label_table(Labels *lbls) {
    if (lbls) {
        free(lbls->data);
        free(lbls);
    }
}

void ensure_label_capacity(Labels *lbls) {
    if (lbls->size >= lbls->capacity) {
        lbls->capacity = lbls->capacity == ZERO ? FOUR : lbls->capacity * TWO;
        Label *new_data = realloc(lbls->data, lbls->capacity * sizeof(Label));
        if (!new_data) {
            perror("Failed to realloc labels table");
            exit(EXIT_FAILURE);
        }
        lbls->data = new_data;
    }
}

/* ---------------- Helpers ---------------- */

static void normalize_label_name(char *s) {
    /* Trim whitespace */
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end-1))) end--;
    *end = NULL_CHAR;

    /* Strip a single trailing ':' if present */
    size_t len = strlen(start);
    if (len && start[len-1] == SEMI_COLON_CHAR) {
        start[len-1] = NULL_CHAR;
    }

    if (start != s) {
        memmove(s, start, strlen(start)+1);
    }
}

/* ---------------- API ---------------- */

void add_label_row(Labels *lbls, const char *label, int table_row_index, LabelTypes label_type, unsigned int is_entry) {
    /* Copy and normalize the label text (remove optional trailing ':', trim spaces) */
    char label_copy[MAX_LABEL_LEN];
    strncpy(label_copy, label, MAX_LABEL_LEN - 1);
    label_copy[MAX_LABEL_LEN - 1] = NULL_CHAR;
    normalize_label_name(label_copy);

    if (strlen(label_copy) > MAX_LABEL_LEN - 1) {
        printf("Label Too Long");
        return;
    }

    if (label_copy[0] && !isupper((unsigned char)label_copy[0])) {
        printf("Invalid Label");
        return;
    }

    ensure_label_capacity(lbls);
    strncpy(lbls->data[lbls->size].label, label_copy, MAX_LABEL_LEN);
    lbls->data[lbls->size].label[LABEL_NULL_CHAR_LOCATION] = NULL_CHAR;

    lbls->data[lbls->size].decimal_address = ZERO;
    lbls->data[lbls->size].table_row_index = table_row_index;
    lbls->data[lbls->size].type = label_type;
    lbls->data[lbls->size].is_entry = is_entry;

    lbls->size++;
}

Label* get_label(Labels *lbls, int index) {
    if (index < ZERO || index >= lbls->size) {
        printf("Invalid index %d\n", index);
        return NULL;
    }
    return &lbls->data[index];
}

int is_label(char *word) {
    size_t n = strlen(word);
    return (n > 0 && word[n - 1] == SEMI_COLON_CHAR) ? TRUE : FALSE;
}

void reset_labels_addresses(Labels *lbls, unsigned int offset) {
    int i;

    for (i = ZERO; i < lbls->size; i++) {
        lbls->data[i].decimal_address = offset + lbls->data[i].table_row_index;
    }
}

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

/* New: find a label by name (ignores an optional trailing ':' on the lookup name) */
Label* find_label_by_name(const Labels *lbls, const char *name) {
    if (!lbls || !name) return NULL;

    char key[MAX_LABEL_LEN];
    strncpy(key, name, MAX_LABEL_LEN - 1);
    key[MAX_LABEL_LEN - 1] = NULL_CHAR;
    /* Normalize key: trim + strip trailing ':' */
    char *start = key;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end-1))) end--;
    *end = NULL_CHAR;
    size_t len = strlen(start);
    if (len && start[len-1] == SEMI_COLON_CHAR) start[len-1] = NULL_CHAR;

    int i;

    for (i = 0; i < lbls->size; i++) {
        if (strcmp(lbls->data[i].label, start) == 0) {
            /* cast away const to match return type; callers should treat as read-only */
            return (Label*)&lbls->data[i];
        }
    }
    return NULL;
}
