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

static volatile sig_atomic_t running = 1;

void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

void set_handler(void(*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(sigNo, &act, NULL) == -1)
        ERR("sigaction");
    return;
}

void usage (char** argv)
{
    printf("USAGE: %s 4 <= N <= 20\n", argv[0]);
    exit(EXIT_FAILURE);
}

void initialize_board(int* board, int N)
{
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            board[i * N + j] = 1 + rand() % 9;
        }
    }
}

void print_board (int* board, int N, pthread_mutex_t* mutex)
{
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
        
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            printf("%d ", board[i * N + j]);
        }
        printf("\n");
    }
    if (pthread_mutex_unlock(mutex))
        ERR("pthread_mutex_unlock");
    printf("\n");
}

int main (int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    set_handler(sigint_handler, SIGINT);

    if (argc != 2)
        usage(argv);
    int N = atoi(argv[1]);
    if (N < 4 || N > 20)
        usage(argv);
    
    printf("My PID is %d\n", getpid());
    char board_name[32];
    snprintf(board_name, 32, "/%d-board", getpid());
    
    int fd;
    if ((fd = shm_open(board_name, O_CREAT | O_TRUNC | O_RDWR, 0666)) < 0)
        ERR("shm_open");

    printf("Shared memory name: %s\n", board_name);
    
    if (ftruncate(fd, 2048))
        ERR("ftruncate");

    void* addr = mmap(NULL, 2048, PROT_READ | PROT_WRITE, MAP_SHARED, 
        fd, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");
    
    char* cursor = (char*)addr;

    int* N_ptr = (int*)cursor;
    cursor += sizeof(int);

    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)cursor;
    cursor += sizeof(pthread_mutex_t);

    int* board_ptr = (int*)cursor;
    cursor += sizeof(int) * N * N;

    *N_ptr = N;
    
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr))
        ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) ||
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST))
        ERR("pthread_mutexattr_setpshared");

    if (pthread_mutex_init(mutex_ptr, &attr))
        ERR("pthread_mutex_init");
    
    if (pthread_mutexattr_destroy(&attr))
        ERR("pthread_mutexattr_destroy");

    
    initialize_board(board_ptr, N);

    while (running)
    {
        print_board(board_ptr, N, mutex_ptr);
        sleep(3);
    }

    printf("SIGINT received, exiting...\n");
    print_board(board_ptr, N, mutex_ptr);
    
    close(fd);
    munmap(addr, 2048);
    if (shm_unlink(board_name) < 0)
        ERR("shm_unlink");
    
    exit(EXIT_SUCCESS);
}