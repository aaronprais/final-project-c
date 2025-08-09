#ifndef ORDERING_INTO_TABLE_H
#define ORDERING_INTO_TABLE_H
#include <stdio.h>
#include "table.h"
#include "labels.h"

int process_file_to_table_and_labels(Table *tbl, Labels *lbls, FILE *file);

#endif //ORDERING_INTO_TABLE_H
