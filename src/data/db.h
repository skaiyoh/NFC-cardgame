//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_DB_H
#define NFC_CARDGAME_DB_H

#include <sqlite3.h>
#include <stdbool.h>

typedef struct {
    sqlite3 *handle;
    bool connected;
    char last_error[256];
} DB;

// Immutable query result: a rows×cols table of nullable strings.
typedef struct {
    int rows;
    int cols;
    char ***data; // data[row][col] — NULL means SQL NULL
} DBResult;

bool db_init(DB *db, const char *path);

void db_close(DB *db);

const char *db_error(DB *db);

// Execute sql (no parameters). Returns NULL on error.
DBResult *db_query(DB *db, const char *sql);

// Execute sql with positional text parameters (?1, ?2, ...). Returns NULL on error.
DBResult *db_query_params(DB *db, const char *sql, int nparams, const char *const *params);

void db_result_free(DBResult *res);

int db_result_rows(const DBResult *res);

const char *db_result_value(const DBResult *res, int row, int col);

bool db_result_isnull(const DBResult *res, int row, int col);

bool db_table_has_column(DB *db, const char *tableName, const char *columnName);

#endif // NFC_CARDGAME_DB_H
