#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "labels.h"

// Initialize a new labels table
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

// Free label table memory
void free_label_table(Labels *lbls) {
    if (lbls) {
        free(lbls->data);
        free(lbls);
    }
}

// Ensure labels table has space
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

// Add a new row
void add_label_row(Labels *lbls, const char *label, int table_row_index, LabelTypes label_type, unsigned int is_entry) {

    char label_copy[strlen(label) + 1];
    strcpy(label_copy, label);
    label_copy[sizeof(label_copy) - 1] = '\0';

    char *token = strtok(label_copy, SPLITTING_DELIM);

    if (strlen(token) > MAX_LABEL_LEN) {
        printf("Label Too Long");
        return;
    }

    if (!isupper(token[0])) {
        printf("Invalid Label");
        return;
    }

    char *rest = strtok(NULL, SPLITTING_DELIM);

    if (rest != NULL) {
        printf("More than one label");
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

// Get pointer to a label by label
Label* get_label(Labels *lbls, int index) {
    if (index < ZERO || index >= lbls->size) {
        printf("Invalid index %d\n", index);
        return NULL;
    }
    return &lbls->data[index];
}

int is_label(char *word) {
    if (word[strlen(word) - 1] == SEMI_COLON_CHAR) {
        return TRUE;
    }
    return FALSE;
}

// Adds 'offset' to decimal_address of every label in the labels
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
