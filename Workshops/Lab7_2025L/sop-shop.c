#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SHOP_FILENAME "/shop"
#define MIN_SHELVES 8
#define MAX_SHELVES 256
#define MIN_WORKERS 1
#define MAX_WORKERS 64

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
    fprintf(stderr, "\t%s n m\n", program_name);
    fprintf(stderr, "\t  n - number of items (shelves), %d <= n <= %d\n", MIN_SHELVES, MAX_SHELVES);
    fprintf(stderr, "\t  m - number of workers, %d <= m <= %d\n", MIN_WORKERS, MAX_WORKERS);
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

void swap(int* x, int* y)
{
    int tmp = *y;
    *y = *x;
    *x = tmp;
}

void shuffle(int* array, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        swap(&array[i], &array[j]);
    }
}

void print_array(int* array, int n)
{
    for (int i = 0; i < n; ++i)
    {
        printf("%3d ", array[i]);
    }
    printf("\n");
}

int main(int argc, char** argv) 
{
    srand(time(NULL));
    if (argc != 3)
        usage(argv[0]);
    
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);

    if (N < 8 || N > 256 || M < 1 || M > 64)
        usage(argv[0]);

    int shm_fd;
    if ((shm_fd = shm_open(SHOP_FILENAME, O_CREAT | O_TRUNC | O_RDWR, 0600)) == -1)
        ERR("shm_open");
    
    int max_size = sizeof(int) * MAX_SHELVES + sizeof(pthread_mutex_t) * MAX_SHELVES;
    if (ftruncate(shm_fd, max_size))
        ERR("ftruncate");
    
    void* addr = mmap(NULL, max_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");

    int* start_arr = (int*)addr;
    pthread_mutex_t* mutexes_start = (pthread_mutex_t*)(start_arr + N);

    pthread_mutexattr_t mutex_attributes;
    if (pthread_mutexattr_init(&mutex_attributes))
        ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&mutex_attributes, PTHREAD_PROCESS_SHARED))
        ERR("pthread_mutexattr_setpshared");
    if (pthread_mutexattr_setrobust(&mutex_attributes, PTHREAD_MUTEX_ROBUST))
        ERR("pthread_mutexattr_setrobust");
    
        
    for (int i = 0; i < N; i++)
    {
        start_arr[i] = i + 1;
        if (pthread_mutex_init(mutexes_start + i, &mutex_attributes))
            ERR("pthread_mutex_init");
    }
    if (pthread_mutexattr_destroy(&mutex_attributes))
        ERR("pthread_mutexattr_destroy");

    shuffle(start_arr, N);
    print_array(start_arr, N);

    pid_t* worker_pids = (pid_t*)malloc(sizeof(pid_t) * M);
    if (!worker_pids) 
        ERR("malloc");

    for (int i = 0; i < M; i++)
    {
        *(worker_pids + i) = fork();
        if (worker_pids[i] == -1)
            ERR("fork");
        if (worker_pids[i] == 0)
        {
            printf("[%d] Worker reports for a night shift\n", getpid());
            srand(time(NULL) ^ getpid());
            int random_idx_less, random_idx_more;

            for (int j = 0; j < 10; j++)
            {
                while (1)
                {
                    random_idx_less = rand() % N;
                    random_idx_more = rand() % N;
                    printf("[%d] idx1 - %d idx2 - %d\n", getpid(), random_idx_less, random_idx_more);
                    if (random_idx_less != random_idx_more)
                        break;
                }

                if (random_idx_less > random_idx_more)
                    swap(&random_idx_less, &random_idx_more);

                if (pthread_mutex_lock(mutexes_start + random_idx_less))
                    ERR("pthread_mutex_lock");
                if (pthread_mutex_lock(mutexes_start + random_idx_more))
                    ERR("pthread_mutex_lock");
                
                if (start_arr[random_idx_less] > start_arr[random_idx_more])
                    swap(&start_arr[random_idx_less], &start_arr[random_idx_more]);

                if (pthread_mutex_unlock(mutexes_start + random_idx_less))
                    ERR("pthread_mutex_unlock");
                if (pthread_mutex_unlock(mutexes_start + random_idx_more))
                    ERR("pthread_mutex_unlock");

                ms_sleep(100);
            }

            free(worker_pids);
            if (munmap(addr, max_size))
                ERR("munmap");
            close(shm_fd);

            exit(EXIT_SUCCESS);
        }
    }

    while (wait(NULL) > 0) {}

    printf("Night shift in Bitronka is over\n");
    print_array(start_arr, N);

    for (int i = 0; i < N; i++)
    {
        if (pthread_mutex_destroy(mutexes_start + i))
            ERR("pthread_mutex_destroy");
    }
    
    if (munmap(addr, max_size))
        ERR("munmap");
    close(shm_fd);
    if (shm_unlink(SHOP_FILENAME))
        ERR("shm_unlink");

    exit(EXIT_SUCCESS);
}
