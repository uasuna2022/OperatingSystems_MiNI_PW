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

    ssize_t mmap_size = sizeof(pthread_barrier_t) + sizeof(pthread_mutex_t) * M * K;
    void* addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, 
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");
    
    pthread_barrierattr_t barrier_attr;
    if (pthread_barrierattr_init(&barrier_attr))
        ERR("pthread_barrierattr_init");
    if (pthread_barrierattr_setpshared(&barrier_attr, PTHREAD_PROCESS_SHARED))
        ERR("pthread_barrierattr_setpshared");
    
    pthread_barrier_t* barrier = (pthread_barrier_t*)addr;
    if (pthread_barrier_init(barrier, &barrier_attr, N + 1))
        ERR("pthread_barrier_init");
    
    if (pthread_barrierattr_destroy(&barrier_attr))
        ERR("pthread_barrierattr_destroy");
    
    pthread_mutex_t* mutexes = (pthread_mutex_t*)(barrier + 1);
    pthread_mutexattr_t mutex_attr;
    if (pthread_mutexattr_init(&mutex_attr))
        ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED))
        ERR("pthread_mutexattr_setpshared");
    if (pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST))
        ERR("pthread_mutexattr_setrobust");
    for (int i = 0; i < M * K; i++)
    {
        if (pthread_mutex_init(&mutexes[i], &mutex_attr))
            ERR("pthread_mutex_init");
    }
    if (pthread_mutexattr_destroy(&mutex_attr))
        ERR("pthread_mutexattr_destroy");


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
            
            if (pthread_barrier_wait(barrier) == PTHREAD_BARRIER_SERIAL_THREAD)
                printf("[%d] All students are ready, starting the cleaning process.\n",
                     getpid());

            // stage 3
            int child_shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0600);
            if (child_shm_fd == -1)
                ERR("shm_open");

            ssize_t child_shm_size = sizeof(double) * M * K;
            
            double* keys = (double*)mmap(NULL, child_shm_size, PROT_READ | PROT_WRITE, 
                MAP_SHARED, child_shm_fd, 0);
            if (keys == MAP_FAILED)
                ERR("mmap");

            for (int j = 0; j < 10; j++)
            {
                int random_keyboard = rand() % M;
                if (sem_wait(semaphores[random_keyboard]))
                    ERR("sem_wait");

                int random_key = rand() % K;
                
                printf("Student [%d]: cleaning the keyboard %d, key %d\n", 
                    getpid(), random_keyboard, random_key);
                ms_sleep(300);

                if (pthread_mutex_lock(&mutexes[random_keyboard * K + random_key]))
                    ERR("pthread_mutex_lock");
                keys[random_keyboard * K + random_key] /= 3.0;
                if (pthread_mutex_unlock(&mutexes[random_keyboard * K + random_key]))
                    ERR("pthread_mutex_unlock");

                if (sem_post(semaphores[random_keyboard]))
                    ERR("sem_post");
            }

            close_semaphores(semaphores, M);
            free(semaphores);
            free(children);
            if (munmap(addr, mmap_size))
                ERR("munmap");
            close(child_shm_fd);
            if (munmap(keys, child_shm_size))
                ERR("munmap");
            
            exit(EXIT_SUCCESS);
        }
    }

    // stage 3
    int fd_shm = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd_shm == -1)
        ERR("shm_open");

    int shm_size = sizeof(double) * M * K;
    if (ftruncate(fd_shm, shm_size))
        ERR("ftruncate");
    
    void* shm_addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (shm_addr == MAP_FAILED)
        ERR("mmap");
    
    double* keys = (double*)shm_addr;
    for (int i = 0; i < M * K; i++)
        keys[i] = 1.0;

    ms_sleep(500);
    if (pthread_barrier_wait(barrier) == PTHREAD_BARRIER_SERIAL_THREAD)
        printf("[%d] All students have started cleaning.\n", getpid());
    
    while (wait(NULL) > 0) {}   

    printf("All students have finished cleaning.\n");
    print_keyboards_state(keys, M, K);

    for (int i = 0; i < M * K; i++)
    {
        if (pthread_mutex_destroy(&mutexes[i]))
            ERR("pthread_mutex_destroy");
    } 
    delete_semaphores(M);
    free(children);
    if (pthread_barrier_destroy(barrier))
        ERR("pthread_barrier_destroy");

    if (munmap(addr, mmap_size))
        ERR("munmap");
    if (munmap(shm_addr, shm_size))
        ERR("munmap");
    if (shm_unlink(SHARED_MEM_NAME))
        ERR("shm_unlink");

    close(fd_shm);  

    exit(EXIT_SUCCESS);
}