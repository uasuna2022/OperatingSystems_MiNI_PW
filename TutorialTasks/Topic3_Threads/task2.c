#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    int* p_counter;
    int* p_containers;
    int containers_size;
    pthread_mutex_t* mutex;
    int n_balls;
} ThreadArguments;

void usage(int argc, char** argv)
{
    printf("USAGE: %s k n\n\t1 <= k <= 10 (number of threads)\n\t100 <= n <= 100000 (number of balls)\n", argv[0]);
    exit(EXIT_FAILURE);
}

void* thread_job (void* arg)
{
    ThreadArguments* args = (ThreadArguments*)arg;
    unsigned int seed = (unsigned int)pthread_self();
    const int N = args->n_balls;
    double pos = 5.0;
    int balls_thrown = 0;

    do {
        pos = 5.0;
        for (int i = 0; i < 10; i++)
        {
            double r = ((double)rand_r(&seed) / RAND_MAX);
            if (r < 0.5)
                pos -= 0.5;
            else pos += 0.5;
        }
        int container = (int)pos;

        pthread_mutex_lock(args->mutex);
        if (*(args->p_counter) >= N)
        {
            pthread_mutex_unlock(args->mutex);
            break;
        }
        (*(args->p_counter))++;
        args->p_containers[container]++;
        printf("[%ld] Ball nr. %d got into busket nr. %d\n", pthread_self(), *(args->p_counter), container);
        pthread_mutex_unlock(args->mutex);
        balls_thrown++;
    } while (1);

    printf("[%ld] Thread finished, threw %d balls\n", pthread_self(), balls_thrown);
    return NULL;  
}

int main (int argc, char** argv)
{
    if (argc != 3)
        usage(argc, argv);
    
    int k = atoi(argv[1]);
    int n = atoi(argv[2]);
    if (k < 1 || k > 10 || n < 100 || n > 100000)
        usage(argc, argv);

    pthread_t* threads = (pthread_t*)malloc(k * sizeof(pthread_t));
    if (!threads)
        ERR("malloc");
    int* balls_counter = (int*)malloc(sizeof(int));
    if (!balls_counter)
    {
        free(threads);
        ERR("malloc");
    }
    *balls_counter = 0;
    int* containers = (int*)malloc(11 * sizeof(int));
    if (!containers)
    {
        free(threads);
        free(balls_counter);
        ERR("malloc");
    }
    for (int i = 0; i < 11; i++)
    {
        *(containers + i) = 0;
    }

    pthread_mutex_t mutex;
    if(pthread_mutex_init(&mutex, NULL))
    {
        free(threads);
        free(balls_counter);
        free(containers);
        ERR("pthread_mutex_init");
    }

    ThreadArguments* thread_args = (ThreadArguments*)malloc(sizeof(ThreadArguments));
    if (!thread_args)
    {
        free(threads);
        free(balls_counter);
        free(containers);
        pthread_mutex_destroy(&mutex);
        ERR("malloc");
    }


    thread_args->containers_size = 11;
    thread_args->p_counter = balls_counter;
    thread_args->p_containers = containers;
    thread_args->mutex = &mutex;
    thread_args->n_balls = n;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < k; i++)
    {
        if (pthread_create(&threads[i], &attr, thread_job, (void*)thread_args))
            ERR("pthread_create");
        printf("[%ld] Created thread %ld\n", pthread_self(), threads[i]);
    }
    pthread_attr_destroy(&attr);

    while (1)
    {
        sleep(1);
        pthread_mutex_lock(&mutex);
        printf("[%ld] Main thread checks is the job finished...\n", pthread_self());
        if (*balls_counter >= n)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }

    for (int i = 0; i < 11; i++)
    {
        printf("Container nr. %d contains %d balls\n", i, containers[i]);
    }

    sleep(1);
    free(threads);
    free(balls_counter);
    free(containers);
    pthread_mutex_destroy(&mutex);
    free(thread_args);

    return EXIT_SUCCESS;
}