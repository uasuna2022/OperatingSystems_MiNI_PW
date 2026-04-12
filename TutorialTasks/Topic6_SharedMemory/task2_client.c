#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))

void usage (char** argv)
{
    printf("USAGE: %s server PID\n", argv[0]);
    exit(EXIT_FAILURE);
}

void make_iteration (int* board, int N, pthread_mutex_t* mutex, 
    int fd, int* score, void* addr)
{
    srand((unsigned int)time(NULL) ^ getpid());
    int rt = pthread_mutex_lock(mutex);
    if (rt != 0)
    {
        if (rt == EOWNERDEAD)
        {
            if (pthread_mutex_consistent(mutex) != 0)
                ERR("pthread_mutex_consistent");
        }
        else
            ERR("pthread_mutex_lock");
    }

    int random_number = 1 + rand() % 10;
    if (random_number == 1)
    {
        printf("Oops...\n");
        close(fd);
        munmap(addr, 2048);
        exit(EXIT_SUCCESS);
    }

    int x = rand() % N;
    int y = rand() % N;
    printf("Trying to search field (%d, %d)\n", x, y);

    int value = board[x * N + y];
    if (value > 0)
    {
        printf("Found %d points!\n", value);
        board[x * N + y] = 0;
        (*score) += value;
        pthread_mutex_unlock(mutex);
        sleep(1);
        return;
    }
    else
    {
        printf("GAME OVER: score %d\n", *score);
        close(fd);
        munmap(addr, 2048);
        exit(EXIT_SUCCESS);
    }
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argv);
    
    int server_pid = atoi(argv[1]);
    char server_name[32];
    snprintf(server_name, 32, "/%d-board", server_pid);
    
    int fd;
    if ((fd = shm_open(server_name, O_RDWR, 0666)) < 0)
        ERR("shm_open");
    
    void* addr = mmap(NULL, 2048, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");

    int N = *((int*)addr);
    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)(addr + sizeof(int));
    int* board_ptr = (int*)(addr + sizeof(int) + sizeof(pthread_mutex_t));


    int my_score = 0;
    while (1)
    {
        make_iteration(board_ptr, N, mutex_ptr, fd, &my_score, addr);
    }
    
    munmap(addr, 2048);
    close(fd);  
    exit(EXIT_SUCCESS);
    
}