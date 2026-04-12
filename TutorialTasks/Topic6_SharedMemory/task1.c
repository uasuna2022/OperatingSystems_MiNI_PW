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

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
    exit(EXIT_FAILURE))

#define MONTECARLO_ITERATIONS 100000
#define LOG_LENGTH 10

void usage (int argc, char** argv)
{
    (void)argc;
    printf("USAGE: %s 1 <= N <= 30\n", argv[0]);
    exit(EXIT_FAILURE);
}

void child_work(int i, float* results, int fd)
{
    int counter = 0;
    double x, y;
    unsigned int seed = (unsigned int)(getpid() ^ time(NULL));
    float result;
    for (int i = 0; i < MONTECARLO_ITERATIONS; i++)
    {
        x = ((double)rand_r(&seed) / RAND_MAX) * 2.0 - 1.0;
        y = ((double)rand_r(&seed) / RAND_MAX) * 2.0 - 1.0;
        if (x * x + y * y <= 1.0)
            counter++;
    }

    result = 4.0f * ((float)counter / (float)MONTECARLO_ITERATIONS);
    results[i] = result;
    char buf[LOG_LENGTH];
    int len = snprintf(buf, LOG_LENGTH, "%8.6f\n", result);
    if (len < 0 || len >= LOG_LENGTH)
        ERR("snprintf");
    if (pwrite(fd, buf, LOG_LENGTH - 1, i * (LOG_LENGTH - 1)) < 0)
        ERR("pwrite");
}


void create_children(int n, float* results, int fd)
{
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        if (pid == 0)
        {
            child_work(i, results, fd);
            exit(EXIT_SUCCESS);
        }
    }
}


int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    int N = atoi(argv[1]);
    if (N > 30 || N < 1)
        usage(argc, argv);
    
    float* results = mmap(NULL, N * sizeof(float), PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (results == MAP_FAILED)
        ERR("mmap");
    
    if (unlink("./log.txt") < 0 && errno != ENOENT)
        ERR("unlink");

    int fd = open("./log.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0)
        ERR("open");
    
    if (ftruncate(fd, N * (LOG_LENGTH - 1)))
        ERR("ftruncate");
    
    create_children(N, results, fd);

    while (wait(NULL) > 0) {}   

    for (int i = 0; i < N; i++)
        printf("Child %d: %f\n", i, results[i]);
    
    close(fd);
    exit(EXIT_SUCCESS);
}