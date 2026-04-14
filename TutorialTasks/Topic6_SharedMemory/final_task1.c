#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))
#define MAX_FILE_SIZE 1048576
#define TOTAL_ASCII_CHARS 128

void usage (char** argv)
{
    printf("USAGE: %s file_name\n", argv[0]);
    exit(EXIT_FAILURE);
}

void count_signs(char* addr, size_t size, int* signs_count)
{
    for (size_t i = 0; i < size; i++)
        signs_count[addr[i]]++;    
}

void print_signs_count(int* signs_count)
{
    printf("ASCII character counts:\n");
    for (int i = 0; i < TOTAL_ASCII_CHARS; i++)
    {
        if (signs_count[i] > 0)
            printf("'%c' (ASCII %d): %d occurrences\n", i, i, signs_count[i]);
    }
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argv);
    
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
        ERR("open");

    struct stat st;
    if (fstat(fd, &st) == -1)
        ERR("fstat");
    
    if (st.st_size > MAX_FILE_SIZE)
    {
        fprintf(stderr, "File is too big\n");
        exit(EXIT_FAILURE);
    }

    char* addr = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");

    printf("File content:\n%s\n", addr);

    int signs_count[TOTAL_ASCII_CHARS] = {0};
    count_signs(addr, st.st_size, signs_count);
    print_signs_count(signs_count);

    if (munmap(addr, st.st_size) == -1)
        ERR("munmap");  
    close(fd);

    exit(EXIT_SUCCESS);
}