#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

#define KEYBOARD_CAP 10
#define SHARED_MEM_NAME "/memory"
#define MIN_STUDENTS KEYBOARD_CAP
#define MAX_STUDENTS 20
#define MIN_KEYBOARDS 1
#define MAX_KEYBOARDS 5
#define MIN_KEYS 5
#define MAX_KEYS KEYBOARD_CAP

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "\t%s n m k\n", program_name);
    fprintf(stderr, "\t  n - number of students, %d <= n <= %d\n", MIN_STUDENTS, MAX_STUDENTS);
    fprintf(stderr, "\t  m - number of keyboards, %d <= m <= %d\n", MIN_KEYBOARDS, MAX_KEYBOARDS);
    fprintf(stderr, "\t  k - number of keys in a keyboard, %d <= k <= %d\n", MIN_KEYS, MAX_KEYS);
    exit(EXIT_FAILURE);
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void print_keyboards_state(double* keyboards, int m, int k)
{
    for (int i=0;i<m;++i)
    {
        printf("Klawiatura nr %d:\n", i + 1);
        for (int j=0;j<k;++j)
            printf("  %e", keyboards[i * k + j]);
        printf("\n\n");
    }
}

void delete_semaphores(int m)
{
    for (int i = 0; i < m; i++)
    {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/sop-sem-%d", i);
        if (sem_unlink(sem_name) == -1)
        {
            if (errno == ENOENT)
                continue;
                // Semaphore doesn't exist, which is fine
            else
                ERR("sem_unlink"); 
        }
    }
}

void create_semaphores(int m, int k)
{
    sem_t* sem;
    for (int i = 0; i < m; i++)
    {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/sop-sem-%d", i);
        sem = sem_open(sem_name, O_CREAT | O_EXCL, 0666, k);
        if (sem == SEM_FAILED)
            ERR("sem_open");
    }
}

void open_semaphores(int m, sem_t** semaphores)
{
    for (int i = 0; i < m; i++)
    {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/sop-sem-%d", i);
        semaphores[i] = sem_open(sem_name, 0);
        if (semaphores[i] == SEM_FAILED)
            ERR("sem_open");
    }
}

void close_semaphores(sem_t** semaphores, int m)
{
    for (int i = 0; i < m; i++)
    {
        if (sem_close(semaphores[i]))
            ERR("sem_close");
    }
}

int main(int argc, char** argv) 
{
    if (argc != 4)
        usage(argv[0]);
    
    int N = atoi(argv[1]), M = atoi(argv[2]), K = atoi(argv[3]);
    if (N > 20 || N < KEYBOARD_CAP || M < 1 || M > 5 || K < 5 || K > KEYBOARD_CAP)
        usage(argv[0]);

    delete_semaphores(M);
    create_semaphores(M, KEYBOARD_CAP);

    pid_t* children = malloc(N * sizeof(pid_t));
    if (children == NULL)
        ERR("malloc");
    
    for (int i = 0; i < N; i++)
    {
        children[i] = fork();
        if (children[i] == -1)
            ERR("fork");
        if (children[i] == 0)
        {
            srand(getpid() ^ time(NULL));   
            sem_t** semaphores = (sem_t**)malloc(sizeof(sem_t*) * M);
            if (!semaphores)
                ERR("malloc");
            
            open_semaphores(M, semaphores);

            for (int j = 0; j < 10; j++)
            {
                int random_keyboard = rand() % M;
                if (sem_wait(semaphores[random_keyboard]))
                    ERR("sem_wait");
                printf("Student [%d]: cleaning the keyboard %d\n", getpid(), random_keyboard);
                ms_sleep(300);
                if (sem_post(semaphores[random_keyboard]))
                    ERR("sem_post");
            }

            close_semaphores(semaphores, M);
            free(semaphores);
            free(children);
            
            exit(EXIT_SUCCESS);
        }
    }
    
    while (wait(NULL) > 0) {}   

    printf("All students have finished cleaning.\n");
    delete_semaphores(M);
    free(children);
    
    exit(EXIT_SUCCESS);
}