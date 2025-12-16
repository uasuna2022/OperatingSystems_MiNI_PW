#define _GNU_SOURCE
#include "backup.h"
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <limits.h>
#include <signal.h>

static volatile sig_atomic_t stop_requested = 0;
static void term_handler(int signo)
{
    (void)signo;
    stop_requested = 1;
}

#define BUFFER_SIZE 1024

struct backup_job {
    pid_t pid;
    char* src;
    char* dst;
};

struct backup_manager {
    struct backup_job* jobs;
    size_t count;
    size_t capacity;
};

struct watch_map_entry {
    int wd;
    char* path;
};
struct watch_manager {
    struct watch_map_entry* entries;
    int inotify_fd;
    size_t count;
    size_t capacity;
};

static void free_job(struct backup_job* job)
{
    if (!job)
        return;

    free(job->src);
    free(job->dst);
    job->src = NULL;
    job->dst = NULL;
    job->pid = -1;
}

// Ensures that backup manager's capacity can hold at least one more job. 
static int ensure_capacity(struct backup_manager* manager)
{
    if (manager->count < manager->capacity)
        return 0;

    size_t new_capacity = manager->capacity == 0 ? 4 : manager->capacity * 2;
    struct backup_job* new_jobs = realloc(manager->jobs, new_capacity * sizeof(struct backup_job));
    if (!new_jobs)
        return -1;

    manager->jobs = new_jobs;
    manager->capacity = new_capacity;
    return 0;
}

backup_manager* backup_manager_create(void)
{
    struct backup_manager* mgr = calloc(1, sizeof(struct backup_manager));
    return mgr;
}

void free_backup_manager(backup_manager* manager)
{
    if (!manager)
        return;

    for (size_t i = 0; i < manager->count; i++)
        free_job(&manager->jobs[i]);

    free(manager->jobs);
    free(manager);
}

static void reap_children(void)
{
    int status = 0;
    while (waitpid(-1, &status, WNOHANG) > 0) {}
}

void backup_manager_terminate_all(backup_manager* manager)
{
    if (!manager)
        return;

    for (size_t i = 0; i < manager->count; i++) {
        if (manager->jobs[i].pid > 0)
            kill(manager->jobs[i].pid, SIGTERM);
    }

    reap_children();
}

static int paths_same(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

static int path_is_prefix(const char* parent, const char* child)
{
    size_t len = strlen(parent);
    return strncmp(parent, child, len) == 0 && (child[len] == '/' || child[len] == '\0');
}

int backup_manager_has_job(backup_manager* manager, const char* src_abs, const char* dst_abs)
{
    if (!manager)
        return 0;

    for (size_t i = 0; i < manager->count; i++) {
        if (paths_same(manager->jobs[i].src, src_abs) && paths_same(manager->jobs[i].dst, dst_abs))
            return 1;
    }
    return 0;
}

// Ensures that the full directory path exists. Creates missing directories
// recursively if necessary. Eventual errors are reported inside err_msg.
static int ensure_directory_exists(const char* path, char** err_msg)
{
    // First of all check if path already exists, if so, validate it's a directory.
    struct stat st;
    if (stat(path, &st) == 0) 
    {
        if (!S_ISDIR(st.st_mode)) 
        {
            if (err_msg)
                asprintf(err_msg, "ERROR: %s is not a directory", path);
            return -1;
        }
        return 0;
    }

    if (errno != ENOENT) 
    {
        if (err_msg)
            asprintf(err_msg, "ERROR: cannot stat %s", path);
        return -1;
    }

    // Modifiedable copy of path is needed for directory creation.
    char* mutable = strdup(path);
    if (!mutable) 
    {
        if (err_msg)
            asprintf(err_msg, "ERROR: out of memory");
        return -1;
    }

    // Remove '/' suffix if present.
    size_t len = strlen(mutable);
    if (len > 1 && mutable[len - 1] == '/')
        mutable[len - 1] = '\0';

    // Main loop to create missing directories recursively.
    for (char* p = mutable + 1; *p; p++) 
    {
        if (*p == '/') 
        {
            *p = '\0';
            if (mkdir(mutable, 0755) == -1 && errno != EEXIST) {
                if (err_msg)
                    asprintf(err_msg, "ERROR: cannot create directory %s", mutable);
                free(mutable);
                return -1;
            }
            *p = '/';
        }
    }

    // Create the final directory after going through all '/' separators.
    if (mkdir(mutable, 0755) == -1 && errno != EEXIST) {
        if (err_msg)
            asprintf(err_msg, "ERROR: cannot create directory %s", mutable);
        free(mutable);
        return -1;
    }

    free(mutable);
    return 0;
}

static int directory_is_empty(const char* path)
{
    DIR* dir = opendir(path);
    if (!dir)
        return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        closedir(dir);
        return 0;
    }

    closedir(dir);
    return 1;
}

// Cleans the path from symlinks, '..' and '.' components, ensuring it exists.
// Used for source directories.
static int normalize_existing_dir(const char* path, char** out_abs, char** err_msg)
{
    char* resolved = realpath(path, NULL);
    if (!resolved) 
    {
        if (err_msg)
            asprintf(err_msg, "ERROR: directory %s does not exist", path);
        return -1;
    }

    struct stat st;
    if (stat(resolved, &st) == -1 || !S_ISDIR(st.st_mode)) {
        if (err_msg)
            asprintf(err_msg, "ERROR: %s is not a directory", path);
        free(resolved);
        return -1;
    }

    *out_abs = resolved;
    return 0;
}

// Cleans the path from symlinks, '..' and '.' components, ensuring it exists or it is 
// being created if missing. Used for destination directories.
static int normalize_destination_dir(const char* path, char** out_abs, char** err_msg)
{
    struct stat st;
    if (stat(path, &st) == 0) 
    {
        if (!S_ISDIR(st.st_mode)) 
        {
            if (err_msg)
                asprintf(err_msg, "ERROR: %s is not a directory", path);
            return -1;
        }
        if (!directory_is_empty(path)) 
        {
            if (err_msg)
                asprintf(err_msg, "ERROR: %s is not empty", path);
            return -1;
        }

        char* resolved = realpath(path, NULL);
        if (!resolved) {
            if (err_msg)
                asprintf(err_msg, "ERROR: directory %s does not exist", path);
            return -1;
        }

        *out_abs = resolved;
        return 0;
    }

    if (errno != ENOENT) {
        if (err_msg)
            asprintf(err_msg, "ERROR: cannot stat %s", path);
        return -1;
    }

    // Create missing directory path
    if (ensure_directory_exists(path, err_msg) == -1)
        return -1;

    char* resolved = realpath(path, NULL);
    if (!resolved) {
        if (err_msg)
            asprintf(err_msg, "ERROR: directory %s does not exist", path);
        return -1;
    }

    *out_abs = resolved;
    return 0;
}

static int is_path_nested(const char* a, const char* b)
{
    return path_is_prefix(a, b) || path_is_prefix(b, a);
}

static backup_add_status add_job_record(struct backup_manager* manager, pid_t pid, 
    const char* src_absolute, const char* dst_absolute)
{
    if (ensure_capacity(manager) == -1)
        return BACKUP_ADD_ERR_INTERNAL;

    struct backup_job* job = &manager->jobs[manager->count];
    job->src = strdup(src_absolute);
    job->dst = strdup(dst_absolute);
    if (!job->src || !job->dst) 
    {
        free_job(job);
        return BACKUP_ADD_ERR_INTERNAL;
    }
    job->pid = pid;
    manager->count++;
    return BACKUP_ADD_OK;
}

static backup_add_status free_and_return(char* src_absolute, char* dst_absolute, backup_add_status code)
{
    free(src_absolute);
    free(dst_absolute);
    return code;
}

// The function to copy a regular file from source_path to destination_path.
static int copy_regular_file(const char* source_path, const char* destination_path,
    char** err, mode_t mode)
{
    int source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1)
       return -1;
    int destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (destination_fd == -1)
    {
        close(source_fd);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0)
    {
        ssize_t bytes_written = write(destination_fd, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            if (err)
                asprintf(err, "ERROR: write error to %s", destination_path);

            close(source_fd);
            close(destination_fd);
            return -1;
        }
    }
    if (bytes_read == -1)
    {
        if (err)
            asprintf(err, "ERROR: read error from %s", source_path);
        close(source_fd);
        close(destination_fd);
        return -1;
    }

    close(source_fd);
    close(destination_fd);
    return 0;
}

// The function to copy a symlink from source_path to destination_path.
// If the symlink is *absolute* and point inside root_source, it is adjusted
// to point inside root_destination instead.
// I.e. if source_path is /src/links/link1 -> /src/somefile.txt
// and root source is /src, then assuming root_destination is /dst,
// the created symlink at destination_path will point to /dst/somefile.txt instead of
// /src/somefile.txt  (/dst/links/link1 -> /dst/somefile.txt)
static int copy_symlink(const char* source_path, const char* destination_path, 
    const char* root_source, const char* root_destination, char** err)
{
    char link_target[BUFFER_SIZE];
    ssize_t read_bytes = readlink(source_path, link_target, BUFFER_SIZE - 1);
    if (read_bytes == -1)
    {
        if (err)
            asprintf(err, "ERROR: cannot read symlink %s", source_path);
        return -1;
    }
    link_target[read_bytes] = '\0';

    if (link_target[0] == '/' && strncmp(link_target, root_source, strlen(root_source)) == 0)
    {
        char new_target[BUFFER_SIZE];
        const char* suffix = link_target + strlen(root_source);
        snprintf(new_target, BUFFER_SIZE, "%s%s", root_destination, suffix);

        if (symlink(new_target, destination_path) == -1)
        {
            if (err)
                asprintf(err, "ERROR: cannot create symlink %s -> %s", destination_path, new_target);
            return -1;
        }
    }
    else 
    {
        if (symlink(link_target, destination_path) == -1)
        {
            if (err)
                asprintf(err, "ERROR: cannot create symlink %s -> %s", destination_path, link_target);
            return -1;
        }
    }

    return 0;
}

// The function to copy a directory recursively from source_path to directory_path.
static int copy_directory_recursive(const char* source_path, const char* directory_path,
    const char* root_source, const char* root_destination, char** err)
{
    DIR* dir = opendir(source_path);
    if (!dir)
    {
        if (err)
            asprintf(err, "ERROR: cannot open directory %s", source_path);
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char source_entry_path[BUFFER_SIZE];
        snprintf(source_entry_path, BUFFER_SIZE, "%s/%s", source_path, entry->d_name);

        char destination_entry_path[BUFFER_SIZE];
        snprintf(destination_entry_path, BUFFER_SIZE, "%s/%s", directory_path, entry->d_name);

        struct stat st;
        if (lstat(source_entry_path, &st) == -1)
        {
            if (err)
                asprintf(err, "ERROR: cannot stat %s", source_entry_path);
            closedir(dir);
            return -1;
        }

        if (S_ISREG(st.st_mode))
        {
            if (copy_regular_file(source_entry_path, destination_entry_path, err, st.st_mode) == -1)
            {
                closedir(dir);
                return -1;
            }
        }
        else if (S_ISLNK(st.st_mode))
        {
            if (copy_symlink(source_entry_path, destination_entry_path, root_source, root_destination, err) == -1)
            {
                closedir(dir);
                return -1;
            }
        }
        else if (S_ISDIR(st.st_mode))
        {
            if (mkdir(destination_entry_path, st.st_mode) == -1 && errno != EEXIST)
            {
                if (err)
                    asprintf(err, "ERROR: cannot create directory %s", destination_entry_path);
                closedir(dir);
                return -1;
            }

            if (copy_directory_recursive(source_entry_path, destination_entry_path,
                root_source, root_destination, err) == -1)
            {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}
static int remove_directory_recursive(const char* path, char** err)
{
    DIR* dir = opendir(path);
    if (!dir) 
    {
        if (err)
            asprintf(err, "ERROR: cannot open directory %s", path);
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) 
        {
            if (err)
                asprintf(err, "ERROR: cannot stat %s", full_path);
            closedir(dir);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) 
            remove_directory_recursive(full_path, err);
        else unlink(full_path);
    }

    closedir(dir);
    return rmdir(path);
}

// Watch manager helper functions
static void watch_manager_add(struct watch_manager* wm, int wd, const char* path, char** err)
{
    if (wm->count == wm->capacity) 
    {
        size_t new_capacity = wm->capacity == 0 ? 4 : wm->capacity * 2;
        struct watch_map_entry* new_entries = realloc(wm->entries, new_capacity * 
            sizeof(struct watch_map_entry));
        if (!new_entries) 
        {
            if (err)
                asprintf(err, "ERROR: out of memory");
            return;
        }

        wm->entries = new_entries;
        wm->capacity = new_capacity;
    }

    wm->entries[wm->count].wd = wd;
    wm->entries[wm->count].path = strdup(path);
    wm->count++;
}

static const char* watch_manager_find_path(struct watch_manager* wm, int wd)
{
    for (size_t i = 0; i < wm->count; i++)
    {
        if (wm->entries[i].wd == wd)
            return wm->entries[i].path;
    }

    return NULL;
}

static char* wm_remove(struct watch_manager* wm, int wd)
{
    for (size_t i = 0; i < wm->count; i++) 
    {
        if (wm->entries[i].wd == wd) 
        {
            char* path = wm->entries[i].path; 
            wm->entries[i] = wm->entries[wm->count - 1];
            wm->count--;
            return path;
        }
    }
    return NULL;
}

static int add_watch_recursive(struct watch_manager* wm, const char* path, char** err)
{
    int wd = inotify_add_watch(wm->inotify_fd, path, 
        IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF);

    if (wd < 0)
        return -1;

    watch_manager_add(wm, wd, path, err);

    DIR* dir = opendir(path);
    if (!dir)
        return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_path[BUFFER_SIZE];
        snprintf(entry_path, BUFFER_SIZE, "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(entry_path, &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode))
            add_watch_recursive(wm, entry_path, err);
    }

    closedir(dir);
    return 0;
}

// Main event handling function. Gets called for each inotify event. 
// Receives the inotify_event structure, the watch manager to resolve paths, 
// the root source and destination paths for symlink adjustments, and an error message pointer.
static void handle_event(struct inotify_event* event, struct watch_manager* wm, 
    const char* root_src, const char* root_dst, char** err) 
{
    if (event->mask & (IN_DELETE_SELF | IN_IGNORED)) 
    {
        if (inotify_rm_watch(wm->inotify_fd, event->wd) == -1) 
        {
            if (err)
                asprintf(err, "ERROR: inotify_rm_watch failed for wd %d", event->wd);
            return;
        }
        char* removed = wm_remove(wm, event->wd);
        if (removed) 
            free(removed);
        
        return;
    }

    if (event->len == 0) 
    {
        if (err)
            asprintf(err, "ERROR: inotify event with no name");
        return;
    }

    // Find a full source path from watch descriptor
    const char* directiry_path = watch_manager_find_path(wm, event->wd);
    if (!directiry_path) 
    {
        if (err)
            asprintf(err, "ERROR: unknown watch descriptor %d", event->wd);
        return;
    }

    // Find full source path
    char full_source_path[PATH_MAX];
    snprintf(full_source_path, sizeof(full_source_path), "%s/%s", directiry_path, event->name);

    // Find full destination path
    char full_destination_path[PATH_MAX];
    const char* relative = directiry_path + strlen(root_src); 
    
    if (*relative == '/') 
        snprintf(full_destination_path, sizeof(full_destination_path), "%s%s/%s",
            root_dst, relative, event->name);
    else snprintf(full_destination_path, sizeof(full_destination_path), "%s/%s",
        root_dst, event->name);
    
    if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) 
    {   
        if (event->mask & IN_ISDIR) 
            remove_directory_recursive(full_destination_path, err);
        else unlink(full_destination_path);
        
        printf("[SYNC] Deleted: %s\n", full_destination_path);
    }
    
    else if ((event->mask & IN_CREATE) || (event->mask & IN_MODIFY) || 
        (event->mask & IN_MOVED_TO) || (event->mask & IN_ATTRIB)) 
    {
        struct stat st;
        if (lstat(full_source_path, &st) == -1) 
        {
            if (err)
                asprintf(err, "ERROR: cannot stat %s", full_source_path);
            return;
        }

        if (S_ISDIR(st.st_mode)) 
        {
            if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) 
            {
                if (mkdir(full_destination_path, st.st_mode) == -1 && errno != EEXIST) 
                {
                    if (err)
                        asprintf(err, "ERROR: cannot create directory %s", full_destination_path);
                    return;
                }
                add_watch_recursive(wm, full_source_path, err);
            }    
        } 
        else if (S_ISREG(st.st_mode)) 
            copy_regular_file(full_source_path, full_destination_path, err, st.st_mode);
        
        else if (S_ISLNK(st.st_mode)) 
            copy_symlink(full_source_path, full_destination_path, root_src, root_dst, err);
        
        printf("[SYNC] Updated: %s\n", full_destination_path);
    }
}

static void run_monitoring_loop(const char* root_src, const char* root_dst, char** err) 
{
    struct watch_manager wm = {0};
    wm.inotify_fd = inotify_init();
    
    if (wm.inotify_fd < 0) 
    {
        if (err)
            asprintf(err, "ERROR: inotify_init failed");
        return;
    }

    add_watch_recursive(&wm, root_src, err);
    printf("[%d] Started monitoring changes...\n", getpid());

    // Buffer aligned to inotify_event structure, to avoid potential alignment issues.
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = term_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    while (!stop_requested) 
    {
        ssize_t len = read(wm.inotify_fd, buffer, sizeof(buffer));
        if (len == -1) {
            if (errno == EINTR) 
                break;
            if (errno == EAGAIN)
                continue;
            if (err)
                asprintf(err, "ERROR: inotify read failed");
            break;
        }

        const char* ptr = buffer;
        while (ptr < buffer + len) 
        {
            struct inotify_event* event = (struct inotify_event*)ptr;
            handle_event(event, &wm, root_src, root_dst, err);
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    
    printf("[%d] Stopped monitoring changes.\n", getpid());
    for (size_t i = 0; i < wm.count; i++) 
    {
        if (inotify_rm_watch(wm.inotify_fd, wm.entries[i].wd) == -1) 
        {
            if (err)
                asprintf(err, "ERROR: inotify_rm_watch failed for wd %d", wm.entries[i].wd);
            continue;
        }
        free(wm.entries[i].path);
    }
    free(wm.entries);
    close(wm.inotify_fd);
}

backup_add_status backup_manager_add_pair(backup_manager* manager, const char* src_raw,
    const char* dst_raw, pid_t* out_child_pid, char** out_src_norm, char** out_dst_norm,
    char** out_error_msg)
{
    if (out_child_pid)
        *out_child_pid = -1;
    if (out_src_norm)
        *out_src_norm = NULL;
    if (out_dst_norm)
        *out_dst_norm = NULL;
    if (out_error_msg)
        *out_error_msg = NULL;

    char* src_absolute = NULL;
    char* dst_absolute = NULL;

    if (normalize_existing_dir(src_raw, &src_absolute, out_error_msg) == -1)
        return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_VALIDATE);

    if (normalize_destination_dir(dst_raw, &dst_absolute, out_error_msg) == -1)
        return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_VALIDATE);

    if (paths_same(src_absolute, dst_absolute)) 
    {
        if (out_error_msg)
            asprintf(out_error_msg, "ERROR: %s and %s are the same directories", src_raw, dst_raw);
        return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_VALIDATE);
    }

    if (is_path_nested(src_absolute, dst_absolute)) 
    {
        if (out_error_msg)
            asprintf(out_error_msg, "ERROR: %s and %s are nested", src_raw, dst_raw);
        return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_VALIDATE);
    }

    if (backup_manager_has_job(manager, src_absolute, dst_absolute)) 
    {
        if (out_error_msg)
            asprintf(out_error_msg, "ERROR: backup %s -> %s already exists", src_absolute, dst_absolute);
        return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_DUPLICATE);
    }

    pid_t pid = fork();
    switch (pid)
    {
        case -1:
        {
            if (out_error_msg)
                asprintf(out_error_msg, "ERROR: fork failed");
            return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_ERR_FORK);
        }
        case 0:
        {
            free_backup_manager(manager);

            struct stat root_st;
            if (stat(src_absolute, &root_st) == 0)
                chmod(dst_absolute, root_st.st_mode);

            if (copy_directory_recursive(src_absolute, dst_absolute,
                src_absolute, dst_absolute, out_error_msg) == -1)
            {
                asprintf(out_error_msg, "ERROR: failed to copy from %s to %s", 
                    src_absolute, dst_absolute);
                
                // TODO: terminate child, free resources properly and inform a parent process.

                exit(EXIT_FAILURE);
            }

            run_monitoring_loop(src_absolute, dst_absolute, out_error_msg);
            exit(EXIT_SUCCESS);
        }
        default:
        {
            backup_add_status status = add_job_record(manager, pid, src_absolute, dst_absolute);
            if (status != BACKUP_ADD_OK) 
            {
                if (out_error_msg)
                    asprintf(out_error_msg, "ERROR: internal error adding job");
                kill(pid, SIGTERM);
                reap_children();
                return free_and_return(src_absolute, dst_absolute, status);
            }

            if (out_child_pid)
                *out_child_pid = pid;
            if (out_src_norm)
                *out_src_norm = strdup(src_absolute);
            if (out_dst_norm)
                *out_dst_norm = strdup(dst_absolute);

            return free_and_return(src_absolute, dst_absolute, BACKUP_ADD_OK);
        }
    } 
}

void list_backups(backup_manager* manager)
{
    if (!manager || manager->count == 0)
    {
        printf("No active backups.\n");
        return;
    }

    printf("Active backups:\n");
    for (size_t i = 0; i < manager->count; i++)
    {
        printf(" (%ld) [%d] %s -> %s\n", i + 1, manager->jobs[i].pid,
             manager->jobs[i].src, manager->jobs[i].dst);
    }
}

// Terminate a specific backup job identified by source and destination paths.
// 0 -> success, -1 -> error, 1 -> not found
int backup_manager_end_pair(backup_manager* manager, const char* src_raw,
    const char* dst_raw, char** out_error_msg)
{
    if (!manager) 
    {
        if (out_error_msg) 
            asprintf(out_error_msg, "ERROR: manager is NULL");
        return -1;
    }

    char* source_absolute = realpath(src_raw, NULL);
    if (!source_absolute) 
    {
        if (out_error_msg) 
            asprintf(out_error_msg, "ERROR: cannot resolve source %s", src_raw);
        return -1;
    }

    char* destination_absolute = realpath(dst_raw, NULL);
    if (!destination_absolute) 
    {
        if (out_error_msg) 
            asprintf(out_error_msg, "ERROR: cannot resolve destination %s", dst_raw);
        free(source_absolute);
        return -1;
    }

    for (size_t i = 0; i < manager->count; i++) {
        if (strcmp(manager->jobs[i].src, source_absolute) == 0 &&
            strcmp(manager->jobs[i].dst, destination_absolute) == 0) 
        {
            pid_t pid = manager->jobs[i].pid;

            if (pid > 0) 
            {
                kill(pid, SIGTERM);
                int status = 0;
                waitpid(pid, &status, 0);
            }

            free_job(&manager->jobs[i]);
            if (i != manager->count - 1)
                manager->jobs[i] = manager->jobs[manager->count - 1];
            manager->count--;

            free(source_absolute);
            free(destination_absolute);
            return 0;
        }
    }

    free(source_absolute);
    free(destination_absolute);

    return 1; // if not found
}