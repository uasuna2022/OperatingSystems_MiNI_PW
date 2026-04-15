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
    if (argc != 3)
        usage(argv[0]);
    
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);

    if (N < 8 || N > 256 || M < 1 || M > 64)
        usage(argv[0]);

    int shm_fd;
    if ((shm_fd = shm_open(SHOP_FILENAME, O_CREAT | O_TRUNC | O_RDWR, 0600)) == -1)
        ERR("shm_open");
    
    int max_size = sizeof(int) * MAX_SHELVES;
    if (ftruncate(shm_fd, max_size))
        ERR("ftruncate");
    
    void* addr = mmap(NULL, max_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED)
        ERR("mmap");

    int* start_arr = (int*)addr;
        
    for (int i = 0; i < N; i++)
        start_arr[i] = i + 1;

    shuffle(start_arr, N);
    print_array(start_arr, N);
    
    if (munmap(addr, max_size))
        ERR("munmap");
    close(shm_fd);
    if (shm_unlink(SHOP_FILENAME))
        ERR("shm_unlink");

    exit(EXIT_SUCCESS);
}
