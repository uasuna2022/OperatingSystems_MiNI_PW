#ifndef SRC_BACKUP_H
#define SRC_BACKUP_H

#include <stddef.h>
#include <sys/types.h>

typedef struct backup_manager backup_manager;

struct watch_manager;

typedef enum {
    BACKUP_ADD_OK = 0,
    BACKUP_ADD_ERR_VALIDATE,
    BACKUP_ADD_ERR_DUPLICATE,
    BACKUP_ADD_ERR_FORK,
    BACKUP_ADD_ERR_INTERNAL
} backup_add_status;

backup_manager* backup_manager_create(void);
void free_backup_manager(backup_manager* manager);
void backup_manager_terminate_all(backup_manager* manager);
int backup_manager_has_job(backup_manager* manager, const char* src_abs, const char* dst_abs);
backup_add_status backup_manager_add_pair(backup_manager* manager, const char* src_raw, 
    const char* dst_raw, pid_t* out_child_pid, char** out_src_norm, char** out_dst_norm,
    char** out_error_msg);
void list_backups(backup_manager* manager);
int backup_manager_end_pair(backup_manager* manager, const char* src_raw, const char* dst_raw, char** out_error_msg);
int backup_manager_restore(struct watch_manager* wm, const char* src_raw, const char* dst_raw,
    char** out_error_msg);

#endif