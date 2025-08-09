#ifndef LABELS_H
#define LABELS_H
#include "util.h"

typedef struct {
    char label[MAX_LABEL_LEN];
    int unsigned table_row_index;
    unsigned int decimal_address;
    LabelTypes type : 2;
    unsigned int is_entry : 1;
} Label;

/* Give the labels table a struct tag so other headers can forward-declare it safely. */
typedef struct Labels {
    Label *data;
    int size;
    int capacity;
} Labels;

/* Construction / destruction */
Labels* create_label_table();
void free_label_table(Labels *lbls);
void ensure_label_capacity(Labels *lbls);

/* Mutation / lookup */
void add_label_row(Labels *lbls, const char *label, int table_row_index, LabelTypes label_type, unsigned int is_entry);
Label* get_label(Labels *lbls, int index);

/* New: find a label by its name (ignores an optional trailing ':') */
Label* find_label_by_name(const Labels *lbls, const char *name);

int is_label(char *word);
void reset_labels_addresses(Labels *lbls, unsigned int offset);
void print_labels(Labels *lbls);

#endif //LABELS_H
