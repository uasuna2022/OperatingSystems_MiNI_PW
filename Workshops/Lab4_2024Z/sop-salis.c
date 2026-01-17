#include "common.h"

typedef struct {
    sigset_t* mask;
    pthread_mutex_t* do_work_mutex;
    int* do_work;
} ThreadArgs;

void* signal_handler_thread (void* arg)
{
    ThreadArgs* args = (ThreadArgs*)arg;
    int signal;
    while (1)
    {
        if (sigwait(args->mask, &signal))
            ERR("sigwait");
        switch (signal)
        {
            case SIGINT:
                printf("[Signal] Caught SIGINT\n");
                pthread_mutex_lock(args->do_work_mutex);
                *(args->do_work) = 0;
                pthread_mutex_unlock(args->do_work_mutex);
                return NULL;
            default:
                continue;
        }
    }

}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    int do_work = 1;
    pthread_mutex_t do_work_mutex = PTHREAD_MUTEX_INITIALIZER;

    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    if (!args)
        ERR("malloc");

    args->do_work = &do_work;
    args->do_work_mutex = &do_work_mutex;
    
    sigset_t mask;
    if (sigemptyset(&mask))
        ERR("sigemptyset");
    if (sigaddset(&mask, SIGINT))
        ERR("sigaddset");
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
        ERR("pthread_sigmask");
    
    args->mask = &mask;
    
    pthread_t signal_thread;
    if (pthread_create(&signal_thread, NULL, signal_handler_thread, (void*)args))
        ERR("pthread_create");
    
    while (1)
    {
        printf("[Main] Working...\n");
        pthread_mutex_lock(&do_work_mutex);
        if (do_work == 0)
        {
            if (pthread_join(signal_thread, NULL))
                ERR("pthread_join");
            pthread_mutex_unlock(&do_work_mutex);
            printf("[Main] do_work = 0 -> terminating...\n");
            break;
        }
        pthread_mutex_unlock(&do_work_mutex);
        msleep(500);
    }

    //usage(argc, argv);
    pthread_mutex_destroy(&do_work_mutex);
    free(args);
    return EXIT_SUCCESS;
}
