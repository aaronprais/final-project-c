#ifndef LABELS_H
#define LABELS_H

#include "util.h"

/*
 * Label
 * -----
 * Represents one symbol in the assembler (like a variable or jump target).
 * Fields:
 *   - label : the text name itself
 *   - table_row_index : points into the instruction/data table
 *   - decimal_address : resolved numeric adress (after base offset)
 *   - type : code/data/ext/unkown
 *   - is_entry : flag if it was declared as .entry
 */
typedef struct {
    char label[MAX_LABEL_LEN];
    unsigned int table_row_index;
    unsigned int decimal_address;
    LabelTypes type : 2;       /* pack into 2 bits just to be fancy (lol) */
    unsigned int is_entry : 1; /* just a single bit flag */
} Label;

/*
 * Labels
 * ------
 * Dynamic array of Label entries.
 * Keeps track of capacity so we can realloc as needed.
 */
typedef struct Labels {
    Label *data;
    int size;
    int capacity;
} Labels;

/* --------- Construction / Destruction --------- */
Labels* create_label_table();
void free_label_table(Labels *lbls);
void ensure_label_capacity(Labels *lbls);

/* --------- Mutation / Lookup --------- */
int add_label_row(Labels *lbls, const char *label, int table_row_index,
                  LabelTypes label_type, unsigned int is_entry,
                  int src_line, const char *src_filename);
Label* get_label(Labels *lbls, int index);

/* find_label_by_name
 * ------------------
 * searches for a label with the same name (ignores optional trailing ':')
 */
Label* find_label_by_name(const Labels *lbls, const char *name);

/* Other helpers */
int is_label(char *word);
void reset_labels_addresses(Labels *lbls, unsigned int offset);
void print_labels(Labels *lbls);
int count_label_by_name(const Labels *lbls, const char *name);

#endif // LABELS_H
