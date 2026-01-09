#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    int* p_L;
    int* p_ready_counter;
    pthread_mutex_t* L_mutex;
    pthread_mutex_t* ready_counter_mutex;
    pthread_mutex_t* stop_mutex;
    bool* stop_flag;
    sigset_t* mask;
} args_str;

void usage (int argc, char** argv)
{
    printf("USAGE: %s n\n\t 2 <= n <= 10\n", argv[0]);
    exit(EXIT_FAILURE);
}

void* thread_work (void* arg)
{
    args_str* args = (args_str*)arg;
    unsigned int seed = (unsigned int)pthread_self() ^ time(NULL);

    int M = 2 + rand_r(&seed) % 99;
    printf("[%ld] Hello from a worker thread! My M = %d\n", pthread_self(), M);
    int lastL = 0;
    int currentL = 1;
    while (1)
    {
        while (lastL == currentL)
        {
            pthread_mutex_lock(args->L_mutex);
            currentL = *(args->p_L);
            pthread_mutex_unlock(args->L_mutex);
            usleep(1000);
        }

        if (currentL % M == 0)
            printf("[%ld] M = %d L = %d L is divisible by M\n", pthread_self(), M, currentL);
            

        pthread_mutex_lock(args->ready_counter_mutex);
        (*(args->p_ready_counter))++;
        pthread_mutex_unlock(args->ready_counter_mutex);
        
        lastL = currentL;
    }

    return NULL;
}

void* sygnal_handler_work (void* arg)
{
    args_str* args = (args_str*)arg;

    int signal;
    while (1)
    {
        if (sigwait(args->mask, &signal))
            ERR("sigwait");
        switch (signal)
        {
            case SIGINT:
                pthread_mutex_lock(args->stop_mutex);
                *(args->stop_flag) = true;
                pthread_mutex_unlock(args->stop_mutex);
                return NULL;
            default:
                return NULL;
        }
    }
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    
    int N = atoi(argv[1]);
    if (N < 2 || N > 10)
        usage(argc, argv);

    sigset_t mask;
    if (sigemptyset(&mask))
        ERR("sigemptyset");
    if (sigaddset(&mask, SIGINT))
        ERR("sigaddset");
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
        ERR("pthread_sigmask"); 
    
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * N);
    if (!threads)
        ERR("malloc");
    
    int L = 1;
    int ready_counter = 0;
    bool stop_flag = false;
    pthread_mutex_t L_mutex;
    pthread_mutex_t ready_counter_mutex;
    pthread_mutex_t stop_mutex;
    if (pthread_mutex_init(&L_mutex, NULL) || pthread_mutex_init(&ready_counter_mutex, NULL)
        || pthread_mutex_init(&stop_mutex, NULL))
    {
        free(threads);
        ERR("pthread_mutex_init");
    }

    args_str* args = (args_str*)malloc(sizeof(args_str));
    if (!args)
    {
        pthread_mutex_destroy(&L_mutex);
        pthread_mutex_destroy(&ready_counter_mutex);
        pthread_mutex_destroy(&stop_mutex);
        free(threads);
        ERR("malloc");
    }

    args->p_L = &L;
    args->p_ready_counter = &ready_counter;
    args->L_mutex = &L_mutex;
    args->ready_counter_mutex = &ready_counter_mutex;
    args->stop_flag = &stop_flag;
    args->stop_mutex = &stop_mutex;
    args->mask = &mask;

    pthread_t signal_thread;
    if (pthread_create(&signal_thread, NULL, sygnal_handler_work, (void*)args))
    {
        pthread_mutex_destroy(&L_mutex);
        pthread_mutex_destroy(&ready_counter_mutex);
        pthread_mutex_destroy(&stop_mutex);
        free(threads);
        free(args);
        ERR("pthread_create");
    }

    for (int i = 0; i < N; i++)
    {
        if (pthread_create(threads + i, NULL, thread_work, (void*)args))
        {
            pthread_mutex_destroy(&L_mutex);
            pthread_mutex_destroy(&ready_counter_mutex);
            pthread_mutex_destroy(&stop_mutex);
            free(threads);
            free(args);
            ERR("pthread_create");
        }
    }

    while (1)
    {
        pthread_mutex_lock(&stop_mutex);
        if (stop_flag)
        {
            pthread_mutex_unlock(&stop_mutex);
            printf("[Main thread] Caught SIGINT, terminating...\n");
            break;
        }
        pthread_mutex_unlock(&stop_mutex);

        bool ready = false;
        while (!ready)
        {
            pthread_mutex_lock(&ready_counter_mutex);
            if (ready_counter == N)  
            {
                ready_counter = 0;
                ready = true;
            }
            pthread_mutex_unlock(&ready_counter_mutex);
        }

        pthread_mutex_lock(&L_mutex);
        L++;
        printf("[Main thread] Current L: %d\n", L);
        pthread_mutex_unlock(&L_mutex);

        usleep(100000);
    }

    for (int i = 0; i < N; i++)
    {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
    }
    pthread_join(signal_thread, NULL);

    free(threads);
    free(args);
    pthread_mutex_destroy(&L_mutex);
    pthread_mutex_destroy(&ready_counter_mutex);
    pthread_mutex_destroy(&stop_mutex);

    exit(EXIT_SUCCESS);
}