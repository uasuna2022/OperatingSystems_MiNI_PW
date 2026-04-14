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
#define TOTAL_ASCII_CHARS 128

void usage (char** argv)
{
    printf("USAGE: %s file_name 3 <= N <= 20\n", argv[0]);
    exit(EXIT_FAILURE);
}

void count_signs(char* addr, int* signs_count, int start_offset, int end_offset)
{
    for (size_t i = start_offset; i < end_offset; i++)
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
    if (argc != 3)
        usage(argv);

    int N = atoi(argv[2]);
    if (N > 20 || N < 3)
        usage(argv);

    int fd_shm = shm_open("/my_shared_memory", O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd_shm == -1)
        ERR("shm_open");
    
    int total_shm_size = sizeof(int) * TOTAL_ASCII_CHARS + sizeof(pthread_mutex_t);

    if (ftruncate(fd_shm, total_shm_size) == -1)
        ERR("ftruncate");
    
    void* addr = mmap(NULL, total_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, 
        fd_shm, 0);
    if (addr == MAP_FAILED) 
        ERR("mmap");

    int* signs_count_addr = (int*)addr;
    pthread_mutex_t* mutex_addr = (pthread_mutex_t*)(addr + 
        sizeof(int) * TOTAL_ASCII_CHARS);
    
    pthread_mutexattr_t mutex_attr;
    if (pthread_mutexattr_init(&mutex_attr))
        ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST))
        ERR("pthread_mutexattr_setrobust");
    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED))
        ERR("pthread_mutexattr_setpshared");
    
    if (pthread_mutex_init(mutex_addr, &mutex_attr))
        ERR("pthread_mutex_init");

    if (pthread_mutexattr_destroy(&mutex_attr))
        ERR("pthread_mutexattr_destroy");

    for (int i = 0; i < TOTAL_ASCII_CHARS; i++)
        signs_count_addr[i] = 0;

    pid_t* child_pids = malloc(N * sizeof(pid_t));
    if (child_pids == NULL)
        ERR("malloc");
    
    for (int i = 0; i < N; i++)
    {
        child_pids[i] = fork();
        if (child_pids[i] < 0)
            ERR("fork");
        if (child_pids[i] == 0)
        {
            srand((unsigned int)(time(NULL) ^ getpid()));

            int fd_file = open(argv[1], O_RDONLY);
            if (fd_file == -1)
                ERR("open");

            struct stat st;
            if (fstat(fd_file, &st) == -1)
                ERR("fstat");
            ssize_t file_size = st.st_size;

            ssize_t bytes_per_child = file_size % N == 0 ? 
                file_size / N : file_size / N + 1;

            int start_offset = i * bytes_per_child;
            int end_offset = (i == N - 1) ? file_size : start_offset + bytes_per_child;

            void* child_addr = mmap(NULL, bytes_per_child, PROT_READ, MAP_PRIVATE, 
                fd_file, 0);
            if (child_addr == MAP_FAILED)
                ERR("mmap");

            int child_signs_count[TOTAL_ASCII_CHARS] = {0};
            count_signs(child_addr, child_signs_count, start_offset, end_offset);
            printf("Child %d counted signs in its portion of the file.\n", i);

            if (munmap(child_addr, bytes_per_child))
                ERR("munmap");
            if (close(fd_file))
                ERR("close");

            int lock_result = pthread_mutex_lock(mutex_addr);
            if (lock_result == EOWNERDEAD)
            {
                if (pthread_mutex_consistent(mutex_addr))
                    ERR("pthread_mutex_consistent");
            }
            else if (lock_result)
            {
                errno = lock_result;
                ERR("pthread_mutex_lock");
            }

            if (rand() % 100 < 3)
                abort();

            for (int j = 0; j < TOTAL_ASCII_CHARS; j++)
                signs_count_addr[j] += child_signs_count[j];
            if (pthread_mutex_unlock(mutex_addr))
                ERR("pthread_mutex_unlock");
            

            if (munmap(addr, total_shm_size))
                ERR("munmap");
            exit(EXIT_SUCCESS);
        }

    }


    int failed = 0;
    int status = 0;
    while (wait(&status) > 0)
    {
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failed = 1;
    }

    if (failed)
        printf("Calculations failed: at least one child process died.\n");
    else
        print_signs_count(signs_count_addr);

    if (pthread_mutex_destroy(mutex_addr))
        ERR("pthread_mutex_destroy");

    if (munmap(addr, total_shm_size))
        ERR("munmap");
    if (close(fd_shm) == -1)
        ERR("close");
    if (shm_unlink("/my_shared_memory"))
        ERR("shm_unlink");
    exit(EXIT_SUCCESS);
}