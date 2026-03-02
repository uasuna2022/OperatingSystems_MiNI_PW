#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MSG_SIZE (PIPE_BUF - sizeof(pid_t))

void usage (int argc, char** argv)
{
    printf("USAGE: %s file fifo\n", argv[0]);
    exit(EXIT_FAILURE);
}

void write_to_fifo (int fifo, int file)
{
    char buffer[PIPE_BUF];
    *((pid_t*)buffer) = getpid();
    char* p = buffer + sizeof(pid_t);
    ssize_t bytes_read;
    ssize_t bytes_wrote;
    do 
    {
        bytes_read = read(file, p, MSG_SIZE);
        if (bytes_read < 0)
            ERR("read");
        if (bytes_read < MSG_SIZE)
            memset(p + bytes_read, 0, MSG_SIZE - bytes_read);
        if (bytes_read > 0)
        {
            bytes_wrote = write(fifo, buffer, PIPE_BUF);
            if (bytes_wrote < 0)
                ERR("write");
        }
    } while (bytes_read == MSG_SIZE);
}

int main (int argc, char** argv)
{
    if (argc != 3)
        usage(argc, argv);
    
    int fifo = mkfifo(argv[2], 0666);
    if (fifo == -1)
        if (errno != EEXIST)
            ERR("mkfifo");
    
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
        ERR("open");
    if ((fifo = open(argv[2], O_WRONLY)) == -1)
        ERR("open_fifo");
    
    write_to_fifo(fifo, fd);
    if (close(fd) == -1)
        ERR("close_file");
    if (close(fifo) == -1)
        ERR("close_fifo");
    
    exit(EXIT_SUCCESS);
}