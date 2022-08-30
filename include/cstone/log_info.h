#ifndef LOG_INFO_H
#define LOG_INFO_H


#ifdef __cplusplus
extern "C" {
#endif

void logdb_dump_raw(LogDB *db, size_t dump_bytes, size_t offset);
void logdb_dump_record(LogDB *db);

// FIXME: This belongs somewhere else
void dump_array_bulk(uint8_t *buf, size_t buf_len, bool show_ascii, bool ansi_color);

#ifdef __cplusplus
}
#endif

#endif // LOG_INFO_H
