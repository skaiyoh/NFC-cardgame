//
// Created by Nathan Davis on 2/16/26.
//

#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool db_init(DB *db, const char *path) {
    if (!db || !path) return false;
    memset(db, 0, sizeof(DB));

    int rc = sqlite3_open(path, &db->handle);
    if (rc != SQLITE_OK) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "sqlite3_open failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "DB connection failed: %s\n", db->last_error);
        sqlite3_close(db->handle);
        db->handle = NULL;
        return false;
    }

    sqlite3_exec(db->handle, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    db->connected = true;
    return true;
}

void db_close(DB *db) {
    if (!db || !db->connected) return;
    sqlite3_close(db->handle);
    db->handle = NULL;
    db->connected = false;
}

const char *db_error(DB *db) {
    if (!db || !db->handle) return "No database connection";
    return sqlite3_errmsg(db->handle);
}

// Collect all rows from a prepared, bound statement into a DBResult.
static DBResult *collect_results(DB *db, sqlite3_stmt *stmt) {
    int cols = sqlite3_column_count(stmt);
    int cap = 8;
    int rows = 0;

    char ***data = malloc((size_t) cap * sizeof(char **));
    if (!data) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (rows == cap) {
            cap *= 2;
            char ***tmp = realloc(data, (size_t) cap * sizeof(char **));
            if (!tmp) {
                for (int i = 0; i < rows; i++) {
                    for (int c = 0; c < cols; c++) free(data[i][c]);
                    free(data[i]);
                }
                free(data);
                sqlite3_finalize(stmt);
                return NULL;
            }
            data = tmp;
        }

        data[rows] = calloc((size_t) cols, sizeof(char *));
        if (!data[rows]) {
            for (int i = 0; i < rows; i++) {
                for (int c = 0; c < cols; c++) free(data[i][c]);
                free(data[i]);
            }
            free(data);
            sqlite3_finalize(stmt);
            return NULL;
        }

        for (int c = 0; c < cols; c++) {
            const unsigned char *val = sqlite3_column_text(stmt, c);
            data[rows][c] = val ? strdup((const char *) val) : NULL;
        }
        rows++;
    }

    if (rc != SQLITE_DONE) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "Step failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "Query failed: %s\n", db->last_error);
        for (int i = 0; i < rows; i++) {
            for (int c = 0; c < cols; c++) free(data[i][c]);
            free(data[i]);
        }
        free(data);
        sqlite3_finalize(stmt);
        return NULL;
    }

    sqlite3_finalize(stmt);

    DBResult *res = malloc(sizeof(DBResult));
    if (!res) {
        for (int i = 0; i < rows; i++) {
            for (int c = 0; c < cols; c++) free(data[i][c]);
            free(data[i]);
        }
        free(data);
        return NULL;
    }
    res->rows = rows;
    res->cols = cols;
    res->data = data;
    return res;
}

DBResult *db_query(DB *db, const char *sql) {
    if (!db || !db->connected || !sql) return NULL;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "Prepare failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "Query failed: %s\n", db->last_error);
        return NULL;
    }

    return collect_results(db, stmt);
}

DBResult *db_query_params(DB *db, const char *sql, int nparams, const char *const *params) {
    if (!db || !db->connected || !sql) return NULL;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "Prepare failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "Param query failed: %s\n", db->last_error);
        return NULL;
    }

    for (int i = 0; i < nparams; i++) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i], -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            snprintf(db->last_error, sizeof(db->last_error),
                     "Bind failed: %s", sqlite3_errmsg(db->handle));
            fprintf(stderr, "Param query failed: %s\n", db->last_error);
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    return collect_results(db, stmt);
}

void db_result_free(DBResult *res) {
    if (!res) return;
    for (int i = 0; i < res->rows; i++) {
        for (int c = 0; c < res->cols; c++) free(res->data[i][c]);
        free(res->data[i]);
    }
    free(res->data);
    free(res);
}

int db_result_rows(const DBResult *res) {
    return res ? res->rows : 0;
}

const char *db_result_value(const DBResult *res, int row, int col) {
    if (!res || row < 0 || row >= res->rows || col < 0 || col >= res->cols) return "";
    return res->data[row][col] ? res->data[row][col] : "";
}

bool db_result_isnull(const DBResult *res, int row, int col) {
    if (!res || row < 0 || row >= res->rows || col < 0 || col >= res->cols) return true;
    return res->data[row][col] == NULL;
}

bool db_table_has_column(DB *db, const char *tableName, const char *columnName) {
    if (!db || !db->connected || !tableName || !columnName) return false;

    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s);", tableName);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "Prepare failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "Column probe failed: %s\n", db->last_error);
        return false;
    }

    bool found = false;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        if (name && strcmp((const char *)name, columnName) == 0) {
            found = true;
            break;
        }
    }

    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        snprintf(db->last_error, sizeof(db->last_error),
                 "Step failed: %s", sqlite3_errmsg(db->handle));
        fprintf(stderr, "Column probe failed: %s\n", db->last_error);
        found = false;
    }

    sqlite3_finalize(stmt);
    return found;
}
