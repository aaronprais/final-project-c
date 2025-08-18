#ifndef FILE_FORMATING_H
#define FILE_FORMATING_H
#include "table.h"
#include "labels.h"
int export_object_file(Table *tbl, const char *name);
int export_entry_file(Labels *lbls, const char *name);
int export_external_file(Table *tbl, Labels *lbls, const char *name);
#endif //FILE_FORMATING_H
