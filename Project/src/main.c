#define _GNU_SOURCE
#include "cli.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))

int main(void)
{
    char* line = NULL;
    size_t line_capacity = 0;
    bool terminate = false;

    while (!terminate) 
    {
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
                // "Add" logic
                break;
            case CLI_COMMAND_END:
                // "End" logic
                break;
            case CLI_COMMAND_LIST:
                // "List" logic
                break;
            case CLI_COMMAND_RESTORE:
                // "Restore" logic
                break;
            case CLI_COMMAND_EXIT:
                terminate = true;
                break;
            }

        // Debug purposes:
        printf("Parsed command type: %d, argc: %zu\n", command.type, command.argc);
        for (size_t i = 0; i < command.argc; i++)
            printf("  argv[%zu]: %s\n", i, command.argv[i]);


        cli_free_cmd(&command);
    }

    free(line);
    return 0;
}