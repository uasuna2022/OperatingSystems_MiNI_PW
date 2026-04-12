#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void create_children(int n, char* ptr)
{
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        if (pid == 0)
        {
            child_work(i, ptr);
            exit(EXIT_SUCCESS);
        }
        else 
        {
            sleep(1);
            printf("%s\n", ptr);
        }
    }
}

void child_work(int i, char* ptr)
{
    ptr[i] = (char)(97 + i);
}

int main(int argc, char** argv)
{
    char* ptr = mmap(NULL, 256, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
        ERR("mmap");
    
    create_children(5, ptr);
    while (wait(NULL) > 0) {}

    exit(EXIT_SUCCESS);
}