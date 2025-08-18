#ifndef FILE_FORMATING_H
#define FILE_FORMATING_H

#include "table.h"
#include "labels.h"

/* Writes table contents into <name>.ob (the object file in base-4) */
int export_object_file(Table *tbl, const char *name);

/* Writes entry symbols into <name>.ent (for .entry lables) */
int export_entry_file(Labels *lbls, const char *name);

/* Writes extern symbol usage into <name>.ext (for .extern lables) */
int export_external_file(Table *tbl, Labels *lbls, const char *name);

#endif // FILE_FORMATING_H
