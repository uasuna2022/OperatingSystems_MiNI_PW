#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define THREADS_COUNT 3

int partial_sums[THREADS_COUNT];
int total_sum = 0;
pthread_barrier_t barrier;

void* thread_job (void* arg)
{
    int number = (int)(intptr_t)arg;
    for (int c = 1; c <= 3; c++)
    {
        unsigned int seed = time(NULL) ^ pthread_self();
        int random_sleep = rand_r(&seed) % 11;
        printf("[%ld] I'm sleeping for %d seconds...\n", pthread_self(), random_sleep);
        sleep(random_sleep);
        int random_value = rand_r(&seed) % 101;
        partial_sums[number] += random_value;
        printf("[%ld] I got number %d\n", pthread_self(), random_value);
        printf("[%ld] I'm at the barrier now. My partial sum is %d\n", pthread_self(), partial_sums[number]);
        if (pthread_barrier_wait(&barrier) == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            total_sum = 0;
            for (int i = 0; i < THREADS_COUNT; i++)
                total_sum += partial_sums[i];
            
            printf("[%ld] I've been chosen as last: total_sum = %d\n", pthread_self(), total_sum);
        }
    }
    return NULL;
}

int main()
{
    if (pthread_barrier_init(&barrier, NULL, THREADS_COUNT))
        ERR("pthread_barrier_init");
    
    pthread_t threads[THREADS_COUNT];
    for (int i = 0; i < THREADS_COUNT; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_job, (void*)(intptr_t)i))
            ERR("pthread_create");
    }

    for (int i = 0; i < THREADS_COUNT; i++)
        pthread_join(threads[i], NULL);

    if(pthread_barrier_destroy(&barrier))
        ERR("pthread_barrier_destroy");
    
    return EXIT_SUCCESS;
}