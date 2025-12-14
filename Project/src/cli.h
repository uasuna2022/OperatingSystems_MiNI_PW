#pragma once
#include <stdlib.h>

typedef enum {
    CLI_COMMAND_ADD,
    CLI_COMMAND_END,
    CLI_COMMAND_LIST,
    CLI_COMMAND_RESTORE,
    CLI_COMMAND_EXIT,
    CLI_NO_COMMAND
} cli_command_type;

typedef struct {
    cli_command_type type;
    size_t argc;
    char** argv;
} cli_cmd;

typedef enum {
    CLI_STATUS_OK = 0,
    CLI_STATUS_EMPTY = 1,
    CLI_STATUS_ERR_UNCLOSED_QUOTE = 2,
    CLI_STATUS_ERR_ALLOC = 4,
    CLI_STATUS_ERR_UNKNOWN_COMMAND = 8,
    CLI_STATUS_ERR_INVALID_ARGS = 16
} cli_status; // TODO: potentially extend with more error codes

cli_status cli_parse_line(const char* line, cli_cmd* out_cmd);
void cli_free_cmd(cli_cmd* cmd);
const char* cli_status_message(cli_status status);
const char* cli_usage_message(cli_command_type type);


