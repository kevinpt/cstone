#ifndef LOG_INDEX_H
#define LOG_INDEX_H

typedef struct {
  dhash hash;
} LogDBIndex;

#ifdef __cplusplus
extern "C" {
#endif

bool logdb_index_update(LogDBIndex *index, LogDBBlock *block, size_t block_start);
bool logdb_index_create(LogDB *db, LogDBIndex *index);
void logdb_index_free(LogDBIndex *index);
bool logdb_index_read(LogDB *db, LogDBIndex *index, uint8_t kind, LogDBBlock *block);

#ifdef __cplusplus
}
#endif

#endif // LOG_INDEX_H
