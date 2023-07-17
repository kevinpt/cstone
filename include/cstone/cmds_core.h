#ifndef CMDS_CORE_H
#define CMDS_CORE_H

typedef bool (*AppBuildInfoCB)(int index, char *buf, size_t buf_size);


extern const ConsoleCommandDef g_core_cmd_set[];
extern AppBuildInfoCB g_report_app_build_info_cb;

#endif
