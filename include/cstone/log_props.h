#ifndef LOG_PROPS_H
#define LOG_PROPS_H

#ifdef __cplusplus
extern "C" {
#endif

bool save_props_to_log(PropDB *db, LogDB *log_db, bool compress);
unsigned restore_props_from_log(PropDB *db, LogDB *log_db);
void update_prng_seed(PropDB *db);

#ifdef __cplusplus
}
#endif

#endif // LOG_PROPS_H
