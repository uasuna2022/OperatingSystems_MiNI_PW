#define _GNU_SOURCE
#include "cli.h"
#include "backup.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))

static volatile sig_atomic_t terminate_requested = 0;
void term_sig_handler(int sig) 
{ 
    (void)sig; 
    terminate_requested = 1; 
}

int main(void)
{
    struct sigaction sa_term;
    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_handler = term_sig_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGINT, &sa_term, NULL);
    sigaction(SIGTERM, &sa_term, NULL);

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigdelset(&mask, SIGCHLD);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    char* line = NULL;
    size_t line_capacity = 0;
    bool terminate = false;
    backup_manager* manager = backup_manager_create();
    if (!manager)
        ERR("backup_manager_create");

    while (!terminate) 
    {
        printf("$ ");
        if (terminate_requested) 
        {
            terminate = true;
            break;
        }
        ssize_t read = getline(&line, &line_capacity, stdin);
        if (read == -1) 
        {
            if (errno == EINTR) 
            {
                if (terminate_requested) 
                {
                    terminate = true;
                    break;
                }
                continue;
            }
            ERR("getline");
        }

        if (read > 0 && line[read - 1] == '\n')
            line[read - 1] = '\0';

        cli_cmd command;
        cli_status status = cli_parse_line(line, &command);
        if (status != CLI_STATUS_OK) 
        {
            const char* message = cli_status_message(status);
            if (message && *message)
                fprintf(stderr, "%s\n", message);

            if (status == CLI_STATUS_ERR_INVALID_ARGS) 
            {
                const char* usage = cli_usage_message(command.type);
                if (usage && *usage)
                    fprintf(stderr, "%s\n", usage);
            }

            continue;
        }

        switch (command.type) {
            case CLI_NO_COMMAND:
                fprintf(stderr, "Warning: no command parsed\n");
                break;
            case CLI_COMMAND_ADD: 
            {
                const char* src_raw = command.argv[0];
                for (size_t i = 1; i < command.argc; i++) 
                {
                    const char* dst_raw = command.argv[i];
                    pid_t child_pid = -1;
                    char* src_norm = NULL;
                    char* dst_norm = NULL;
                    char* err_msg = NULL;

                    backup_add_status result = backup_manager_add_pair(manager, src_raw, dst_raw, 
                        &child_pid, &src_norm, &dst_norm, &err_msg);

                    switch (result) 
                    {
                        case BACKUP_ADD_OK:
                            printf("Added backup %s -> %s (pid %d)\n", src_norm ? src_norm : src_raw, 
                                dst_norm ? dst_norm : dst_raw, child_pid);
                            break;
                        case BACKUP_ADD_ERR_DUPLICATE:
                            fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: duplicate backup");
                            break;
                        case BACKUP_ADD_ERR_VALIDATE:
                            fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: validation failed");
                            break;
                        case BACKUP_ADD_ERR_FORK:
                            fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: fork failed");
                            break;
                        case BACKUP_ADD_ERR_INTERNAL:
                            fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: add failed");
                            break;
                    }

                    free(src_norm);
                    free(dst_norm);
                    free(err_msg);
                }
                break;
            }
            case CLI_COMMAND_END:
            {
                const char* src_raw = command.argv[0];
                for (size_t i = 1; i < command.argc; i++) 
                {
                    const char* dst_raw = command.argv[i];
                    char* err_msg = NULL;
                    int res = backup_manager_end_pair(manager, src_raw, dst_raw, &err_msg);
                    if (res == 0) 
                        printf("Ended backup %s -> %s\n", src_raw, dst_raw);
                    else if (res == 1) 
                        fprintf(stderr, "Warning: backup %s -> %s not found\n", src_raw, dst_raw);
                    else fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: failed to end backup");
                    
                    free(err_msg);
                }
                break;
            }
            case CLI_COMMAND_LIST:
                list_backups(manager);
                break;
            case CLI_COMMAND_RESTORE:
            {
                const char* src_raw = command.argv[0];
                const char* dst_raw = command.argv[1];
                char* err_msg = NULL;

                if (backup_manager_restore(NULL, src_raw, dst_raw, &err_msg) == -1) 
                    fprintf(stderr, "%s\n", err_msg ? err_msg : "ERROR: restore failed");
                free(err_msg);
                break;
            }

            case CLI_COMMAND_EXIT:
                terminate = true;
                break;
            }

        cli_free_cmd(&command);
    }

    free(line);
    backup_manager_terminate_all(manager);
    free_backup_manager(manager);
    return 0;
}