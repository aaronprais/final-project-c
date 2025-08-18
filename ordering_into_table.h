#ifndef ORDERING_INTO_TABLE_H
#define ORDERING_INTO_TABLE_H
#include <stdio.h>
#include "table.h"
#include "labels.h"

/*
 * process_file_to_table_and_labels
 * --------------------------------
 * Reads one source file line-by-line and populates:
 *   - tbl  : the Table of rows (each row describes a thing the assembler needs)
 *   - lbls : the Labels struct (holds label infos)
 *   - file : already-opened FILE* you pass in
 *   - src_filename : only for error messages (so users see wich file had the issue)
 *
 * returns: 0 if no errors, non-zero if there were parsing errs.
 * note: doesn’t own/close 'file'. Caller should close it when done.
 */
int process_file_to_table_and_labels(Table *tbl, Labels *lbls, FILE *file, const char *src_filename);

#endif /* ORDERING_INTO_TABLE_H */
