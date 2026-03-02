#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define PIPE_BUF 4096

void usage (int argc, char** argv)
{
    printf("USAGE: %s fifo_name\n", argv[0]);
    exit(EXIT_FAILURE);
}

void read_from_fifo (int fifo)
{
    ssize_t bytes_read;
    char c;
    do 
    {
        bytes_read = read(fifo, &c, 1);
        if (bytes_read == -1)
            ERR("read");
        if (bytes_read > 0 && isalnum(c))
            printf("%c", c);
    } while (bytes_read > 0);
    printf("\n");
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    
    if (mkfifo(argv[1], 0666) == -1)
        if (errno != EEXIST)
            ERR("mkfifo");
    
    int fifo = open(argv[1], O_RDONLY);
    if (fifo == -1)
        ERR("open");
    
    read_from_fifo(fifo);
    if (close(fifo) == -1)
        ERR("close");
    
    return EXIT_SUCCESS;
}