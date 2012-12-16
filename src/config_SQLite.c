#include "common.h"

#if ENABLE_SQLITE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "config.h"
#include "log.h"
#include "misc.h"

extern sqlite3 *db;
static sqlite3_stmt *stmt_get;
static sqlite3_stmt *stmt_set;
static char table_name[10];

/*
 * Set initial configuration and prepare get/set statements.
 *
 * Returns 1 if successful, 0 if not.
 */
int
cfg_read(const char *source_table)
{
        assert(stmt_get == NULL && stmt_set == NULL);
        
        assert(source_table && strlen(source_table) < sizeof(table_name));
        strcpy(table_name, source_table);
                
        /*
         * Cache some configuration.
         */
        config.fullscreen = 0;
        config.flip = 0;
        config.pixel_scale = 1;
        config.FPSUpdateInterval = 500;
        config.use_desktop = 0;
        config.gameSpeed = 0;
        config.defaultShapeColor = color_32bit(0.0, 1.0, 0.0, 1.0);
        config.screen_width = 960;
        config.screen_height = 640;
        config.window_width = 480;
        config.window_height = 320;
        config.download_update = 0;
        
        config.grid_info = 0;
        config.grid_expand = 0;
        config.grid_many = 10;
        
        /*
         * XXX The above settings are for non-retina iPhone.
         * Here are the changes for retina:
         *
         * config.retina = 1
         * config.pixel_scale = 1
         * config.screen_width = 960
         * config.screen_height = 640
         *
         * Note: config values must not be used in scripts!
         */
        
        /* Prepare statement that will fetch config values. */
        assert(stmt_get == NULL);
        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT value FROM %s WHERE key=?",
                 source_table);
        sqlite3_prepare_v2(db, sql, -1, &stmt_get, NULL);
        if (stmt_get == NULL) {
                log_warn("[%s] %s", sql, sqlite3_errmsg(db));
                return 0;
        }
        
        /* Prepare statement that will set config values. */
        assert(stmt_set == NULL);
        snprintf(sql, sizeof(sql), "UPDATE %s SET value=? WHERE key=?",
                 source_table);
        sqlite3_prepare_v2(db, sql, -1, &stmt_set, NULL);
        if (stmt_set == NULL) {
                log_warn("[%s] %s", sql, sqlite3_errmsg(db));
                return 0;
        }
        return 1;
}

void
cfg_close()
{
        if (stmt_get != NULL) {
                sqlite3_finalize(stmt_get);
                stmt_get = NULL;
        }
        if (stmt_set != NULL) {
                sqlite3_finalize(stmt_set);
                stmt_set = NULL;
        }
}

int
cfg_has_key(const char *key)
{
        assert(key && *key != '\0');
        
        /* Bind key parameter. */
        sqlite3_bind_text(stmt_get, 1, key, -1, SQLITE_STATIC);
        
        /* Run statement. */
        int rc = sqlite3_step(stmt_get);
        sqlite3_reset(stmt_get);
        
        return (rc == SQLITE_ROW);
}

float
cfg_get_float(const char *key)
{
        abort();
}

void
cfg_set_str(const char *key, const char *value)
{
        assert(key && value && stmt_set);
        
        /* Bind parameters. */
        sqlite3_bind_text(stmt_set, 1, value, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_set, 2, key, -1, SQLITE_STATIC);
        
        /* Run statement. */
        RCCHECK(sqlite3_step(stmt_set), SQLITE_DONE);
        
        /* Reset statement so we can reuse it later. */
        sqlite3_reset(stmt_set);
}

void
cfg_get_cstr(const char *key, char *buf, uint bufsize)
{
        assert(key && bufsize > 0 && buf && stmt_get);
        
        /* Bind key parameter. */
        sqlite3_bind_text(stmt_get, 1, key, -1, SQLITE_STATIC);
        
        /* Run statement. */
        RCCHECK(sqlite3_step(stmt_get), SQLITE_ROW);
        
        /* Extract and store value. */
        const char *value = (const char *)sqlite3_column_text(stmt_get, 0);
        assert(strlen(value) < bufsize);
        strcpy(buf, value);
        
        /* Reset statement so we can reuse it later. */
        sqlite3_reset(stmt_get);
}

uint32_t
cfg_get_color(const char *key)
{
        abort();
}

int
cfg_get_int(const char *key)
{
        abort();
}

int
cfg_get_bool(const char *key)
{
        abort();
}

#endif  /* ENABLE_SQLITE */
