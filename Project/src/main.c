#define _GNU_SOURCE
#include "cli.h"
#include "backup.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

static void print_prompt_stdout(void)
{
    char* cwd = getcwd(NULL, 0);
    if (cwd) 
    {
        printf("[User]:%s$  ", cwd);
        free(cwd);
    } 
    else printf("[User]:?$  ");
    fflush(stdout);
}

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))

int main(void)
{
    char* line = NULL;
    size_t line_capacity = 0;
    bool terminate = false;
    backup_manager* manager = backup_manager_create();
    if (!manager)
        ERR("backup_manager_create");

    while (!terminate) 
    {
        print_prompt_stdout();
        ssize_t read = getline(&line, &line_capacity, stdin);
        if (read == -1)
            ERR("getline");

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
                // "End" logic
                break;
            case CLI_COMMAND_LIST:
                list_backups(manager);
                break;
            case CLI_COMMAND_RESTORE:
                // "Restore" logic
                break;
            case CLI_COMMAND_EXIT:
                terminate = true;
                break;
            }

        /*
        printf("Parsed command type: %d, argc: %zu\n", command.type, command.argc);
        for (size_t i = 0; i < command.argc; i++)
            printf("  argv[%zu]: %s\n", i, command.argv[i]);
        */

        cli_free_cmd(&command);
    }

    free(line);
    backup_manager_terminate_all(manager);
    free_backup_manager(manager);
    return 0;
}