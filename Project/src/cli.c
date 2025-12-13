#include "cli.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char** data;
    size_t count;
    size_t capacity;
} token_buffer;

// "static" keyword indicates that these functions are private to this file, so
// they won't be visible outside of cli.c. (to avoid potential name clashes)

// Resets the token buffer, freeing all allocated tokens and the buffer itself.
static void tokens_reset(token_buffer* buffer)
{
    if (!buffer)
        return;

    for (size_t i = 0; i < buffer->count; i++) 
        free(buffer->data[i]);

    free(buffer->data);
    buffer->data = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}

// Pushes a new token into the token buffer, resizing if necessary.
static cli_status tokens_push(token_buffer* buffer, char* token)
{
    if (buffer->count == buffer->capacity) 
    {
        size_t new_capacity = buffer->capacity == 0 ? 4 : buffer->capacity * 2;
        char** new_data = realloc(buffer->data, new_capacity * sizeof(char*));
        if (!new_data) 
        {
            free(token);
            return CLI_STATUS_ERR_ALLOC;
        }
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    buffer->data[buffer->count] = token;
    buffer->count++;

    return CLI_STATUS_OK;
}

// Skips whitespace characters and returns the next non-whitespace character position.
static const char* skip_whitespace(const char* cursor)
{
    while (cursor && *cursor && isspace((unsigned char)*cursor))
        cursor += sizeof(char);
    
    return cursor;
}

// Copies a segment of the input line into a newly allocated string.
static char* copy_segment(const char* start, size_t length)
{
    char* result = malloc((length + 1) * sizeof(char));
    if (!result)
        return NULL;

    if (length > 0) 
        memcpy(result, start, length);
    result[length] = '\0';

    return result;
}

// Parses a quoted token from the input line.
static cli_status parse_quoted(const char** cursor, char quote, token_buffer* buffer)
{
    const char* start = *cursor;
    const char* walker = start;

    while (*walker && *walker != quote)
        walker += sizeof(char);

    if (*walker != quote)
        return CLI_STATUS_ERR_UNCLOSED_QUOTE;

    size_t length = (size_t)(walker - start);
    char* token = copy_segment(start, length);
    if (!token)
        return CLI_STATUS_ERR_ALLOC;

    cli_status status = tokens_push(buffer, token);
    *cursor = walker + sizeof(char);

    return status;
}

// Parses an unquoted token from the input line.
static cli_status parse_unquoted(const char** cursor, token_buffer* buffer)
{
    const char* start = *cursor;
    const char* walker = start;

    while (*walker && !isspace((unsigned char)*walker))
        walker += sizeof(char);

    size_t length = (size_t)(walker - start);
    char* token = copy_segment(start, length);
    if (!token)
        return CLI_STATUS_ERR_ALLOC;

    cli_status status = tokens_push(buffer, token);
    *cursor = walker;

    return status;
}

// Initializes a cli_cmd structure to default values.
static void init_cmd(cli_cmd* cmd)
{
    if (!cmd) 
        return;

    cmd->argc = 0;
    cmd->argv = NULL;
    cmd->type = CLI_NO_COMMAND;
}

// Main function to parse a command line into a cli_cmd structure.
cli_status cli_parse_line(const char* line, cli_cmd* out_cmd)
{
    if (!out_cmd)
        return CLI_STATUS_ERR_INVALID_ARGS;
    
    init_cmd(out_cmd);

    if (!line)
        return CLI_STATUS_EMPTY;

    token_buffer buffer = {0};
    const char* cursor = skip_whitespace(line);

    while (cursor && *cursor) 
    {
        if (*cursor == '\'') // if a single quote (') is found 
        {
            cursor += sizeof(char);
            cli_status status = parse_quoted(&cursor, '\'', &buffer);
            if (status != CLI_STATUS_OK) 
            {
                tokens_reset(&buffer);
                return status;
            }
        } 
        else if (*cursor == '"')  // if a double quote (") is found
        {
            cursor += sizeof(char);
            cli_status status = parse_quoted(&cursor, '"', &buffer);
            if (status != CLI_STATUS_OK) 
            {
                tokens_reset(&buffer);
                return status;
            }
        } 
        else 
        {
            cli_status status = parse_unquoted(&cursor, &buffer);
            if (status != CLI_STATUS_OK) 
            {
                tokens_reset(&buffer);
                return status;
            }
        }

        cursor = skip_whitespace(cursor);
    }

    if (buffer.count == 0) 
    {
        tokens_reset(&buffer);
        return CLI_STATUS_EMPTY;
    }

    char* command_token = buffer.data[0];

    cli_command_type command_type;
    if (strcmp(command_token, "add") == 0) 
        command_type = CLI_COMMAND_ADD;
    else if (strcmp(command_token, "end") == 0) 
        command_type = CLI_COMMAND_END;
    else if (strcmp(command_token, "list") == 0) 
        command_type = CLI_COMMAND_LIST;
    else if (strcmp(command_token, "restore") == 0) 
        command_type = CLI_COMMAND_RESTORE;
    else if (strcmp(command_token, "exit") == 0) 
        command_type = CLI_COMMAND_EXIT;
    else 
    {
        tokens_reset(&buffer);
        return CLI_STATUS_ERR_UNKNOWN_COMMAND;
    }

    size_t arg_count = buffer.count - 1;

    cli_status status = CLI_STATUS_OK;

    switch (command_type) 
    {
    case CLI_COMMAND_ADD:
        if (arg_count < 2)
            status = CLI_STATUS_ERR_INVALID_ARGS;
        break;
    case CLI_COMMAND_END:
        if (arg_count < 2)
            status = CLI_STATUS_ERR_INVALID_ARGS;
        break;
    case CLI_COMMAND_LIST:
        if (arg_count != 0) 
            status = CLI_STATUS_ERR_INVALID_ARGS;
        break;
    case CLI_COMMAND_RESTORE:
        if (arg_count != 2) 
            status = CLI_STATUS_ERR_INVALID_ARGS;
        break;
    case CLI_COMMAND_EXIT:
        if (arg_count != 0) 
            status = CLI_STATUS_ERR_INVALID_ARGS;
        break;
    case CLI_NO_COMMAND:
        status = CLI_STATUS_ERR_UNKNOWN_COMMAND;
        break;
    default:
        status = CLI_STATUS_ERR_UNKNOWN_COMMAND;
        break;
    }

    if (status != CLI_STATUS_OK) 
    {
        tokens_reset(&buffer);
        return status;
    }

    free(command_token);

    char** arguments = NULL;
    if (arg_count > 0) 
    {
        arguments = malloc(arg_count * sizeof(char*));
        if (!arguments) 
        {
            tokens_reset(&buffer);
            return CLI_STATUS_ERR_ALLOC;
        }

        for (size_t i = 0; i < arg_count; i++)
            arguments[i] = buffer.data[i + 1];
    }

    free(buffer.data);

    out_cmd->type = command_type;
    out_cmd->argc = arg_count;
    out_cmd->argv = arguments;

    return CLI_STATUS_OK;
}

// Frees the resources allocated for a cli_cmd structure.
void cli_free_cmd(cli_cmd* cmd)
{
    if (!cmd)
        return;
    if (cmd->argv)
    {
        for (size_t i = 0; i < cmd->argc; i++)
            free(cmd->argv[i]);

        free(cmd->argv);
    }

    init_cmd(cmd);
}

// Returns an appropriate message string 
const char* cli_status_message(cli_status status)
{
    switch (status) {
    case CLI_STATUS_OK:
        return "";
    case CLI_STATUS_EMPTY:
        return "Warning: empty command";
    case CLI_STATUS_ERR_UNCLOSED_QUOTE:
        return "Error: unclosed quote in command";
    case CLI_STATUS_ERR_ALLOC:
        return "Error: memory allocation failed";
    case CLI_STATUS_ERR_UNKNOWN_COMMAND:
        return "Error: unknown command";
    case CLI_STATUS_ERR_INVALID_ARGS:
        return "Error: invalid number of arguments for command";
    default:
        return "Unknown error";
    }
}
