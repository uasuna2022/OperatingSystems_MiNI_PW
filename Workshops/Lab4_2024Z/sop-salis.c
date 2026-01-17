#include "common.h"

typedef struct {
    sigset_t* mask;
    pthread_mutex_t* do_work_mutex;
    int* do_work;
} ThreadArgs;

typedef struct {
    int unused;
    int sprinkled;
} Region;

typedef struct {
    Region* regions;
    pthread_mutex_t* regions_mutexes;
    pthread_mutex_t* do_work_mutex;
    int* do_work;
    int* N;
} PorterArgs;

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

void* porter_work (void* arg)
{
    PorterArgs* args = (PorterArgs*)arg;
    unsigned int seed = time(NULL) ^ pthread_self();

    while (1)
    {
        pthread_mutex_lock(args->do_work_mutex);
        if (*(args->do_work) == 0)
        {
            pthread_mutex_unlock(args->do_work_mutex);
            printf("[%ld] Porter is leaving...\n", pthread_self());
            return NULL;
        }
        pthread_mutex_unlock(args->do_work_mutex);

        int random_region = rand_r(&seed) % *(args->N);
        msleep(5 + random_region);
        printf("[%ld] Porter brought 5 bags of salt to field nr. %d\n", pthread_self(), random_region);

        pthread_mutex_lock(&(args->regions_mutexes[random_region]));
        args->regions[random_region].unused += 5;
        pthread_mutex_unlock(&(args->regions_mutexes[random_region]));
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    if (argc != 3)
        usage(argc, argv);
    int N = atoi(argv[1]);
    int Q = atoi(argv[2]);
    if (N < 1 || N > 20 || Q < 1 || Q > 10)
        usage(argc, argv);

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


    Region* regions = (Region*)malloc(sizeof(Region) * N);
    if (!regions)
        ERR("malloc");
    for (int i = 0; i < N; i++)
    {
        regions[i].sprinkled = 0;
        regions[i].unused = 0;
    }

    pthread_t* porters = (pthread_t*)malloc(sizeof(pthread_t) * Q);
    if (!porters)
        ERR("malloc");

    PorterArgs* porter_args = (PorterArgs*)malloc(sizeof(PorterArgs));
    if (!porter_args)
        ERR("porter_args");
    
    porter_args->do_work = &do_work;
    porter_args->do_work_mutex = &do_work_mutex;
    porter_args->N = &N;
    porter_args->regions = regions;

    pthread_mutex_t* regions_mutexes = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * N);
    if (!regions_mutexes)
        ERR("malloc");
    
    for (int i = 0; i < N; i++)
        if (pthread_mutex_init(&regions_mutexes[i], NULL))
            ERR("pthread_mutex_init");
    
    porter_args->regions_mutexes = regions_mutexes;

    for (int i = 0; i < Q; i++)
    {
        if (pthread_create(&porters[i], NULL, porter_work, (void*)porter_args))
            ERR("pthread_create");
    }
    
    
    while (1)
    {
        printf("[Main] Working...\n");
        pthread_mutex_lock(&do_work_mutex);
        if (do_work == 0)
        {
            pthread_mutex_unlock(&do_work_mutex);
            if (pthread_join(signal_thread, NULL))
                ERR("pthread_join");
            for (int i = 0; i < Q; i++)
            {
                if (pthread_join(porters[i], NULL))
                    ERR("pthread_join");
            }
            printf("[Main] do_work = 0 -> terminating...\n");
            break;
        }
        pthread_mutex_unlock(&do_work_mutex);
        msleep(500);
    }

    for (int i = 0; i < N; i++)
    {
        printf("%d: %d %d\n", i, regions[i].unused, regions[i].sprinkled);
    }

    pthread_mutex_destroy(&do_work_mutex);
    for (int i = 0; i < N; i++)
        if (pthread_mutex_destroy(&regions_mutexes[i]))
            ERR("pthread_mutex_destroy");
    free(regions_mutexes);
    free(porter_args);
    free(porters);
    free(args);
    free(regions);
    return EXIT_SUCCESS;
}
