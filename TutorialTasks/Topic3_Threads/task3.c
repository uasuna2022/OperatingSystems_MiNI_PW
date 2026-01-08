#define _GNU_SOURCE 
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    int value;
    struct LItem* next;
} LItem;

typedef struct {
    int size;
    LItem* head;
} List;

void add_to_list (List** l, int value)
{
    List* list = *l;
    if (list == NULL)
        return;

    LItem* new_item = (LItem*)malloc(sizeof(LItem));
    if (!new_item)
        ERR("malloc");
    new_item->value = value;
    new_item->next = NULL;
    if (list->size == 0)
        list->head = new_item;
    
    else
    {
        LItem* pointer = list->head;
        while (pointer->next != NULL)
            pointer = pointer->next;
        
        pointer->next = new_item;
    }

    list->size++;
    return;
}

LItem* delete_random (List** l)
{
    List* list = *l;
    if (list == NULL)
        return NULL;
    if (list->size == 0)
        return NULL;
    
    unsigned int seed = time(NULL) ^ (unsigned int)pthread_self();
    int number_to_delete = rand_r(&seed) % list->size;
    LItem* p = list->head;
    LItem* pprev = NULL;
    for (int i = 0; i < number_to_delete; i++)
    {
        pprev = p;
        p = p->next;
    }
    LItem* pnext = p->next;

    if (p != list->head)
        pprev->next = pnext; 
    else list->head = pnext;

    p->next = NULL;
    list->size--;
    return p;
}

void usage(int argc, char** argv)
{
    printf("USAGE: %s k\n\t3 <= k <= 15\n", argv[0]);
    exit(EXIT_FAILURE);
}

typedef struct 
{
    int* quit_flag;
    pthread_mutex_t* quit_flag_mutex;
    List** main_list;
    pthread_mutex_t* main_list_mutex;
    sigset_t* mask;
} ThreadArgs;

void* signal_handler_work (void* arg)
{
    ThreadArgs* targs = (ThreadArgs*)arg;
    
    int signal;
    while (1)
    {
        if (sigwait(targs->mask, &signal))
            ERR("sigwait");
        switch (signal)
        {
            case SIGINT:
                pthread_mutex_lock(targs->main_list_mutex);
                LItem* deleted_item = delete_random(targs->main_list);
                if (deleted_item == NULL)
                {
                    pthread_mutex_unlock(targs->main_list_mutex);
                    printf("[%ld] List is empty, nothing to delete!\n", pthread_self());
                    break;
                }
                printf("[%ld] Number %d has been deleted!\n", pthread_self(), deleted_item->value);
                free(deleted_item);
                pthread_mutex_unlock(targs->main_list_mutex);
                break;
            case SIGQUIT:
                pthread_mutex_lock(targs->quit_flag_mutex);
                *(targs->quit_flag) = 1;
                pthread_mutex_unlock(targs->quit_flag_mutex);
                pthread_exit(NULL);
            default:
                return NULL;
        }    
    }

    return NULL;
}

void delete_list (List** l)
{
    List* list = *l;
    while (list->size != 0)
    {
        LItem* deleted_item = delete_random(l);
        free(deleted_item);
    }
    free(list);
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    
    int k = atoi(argv[1]);
    if (k > 15 || k < 3)
        usage(argc, argv);

    sigset_t mask;
    if (sigemptyset(&mask))
        ERR("sigemptyset");
    if (sigaddset(&mask, SIGINT) || sigaddset(&mask, SIGQUIT))
        ERR("sigaddset");
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
        ERR("pthread_sigmask");
    

    printf("PID: %d\n", getpid());
    List* main_list = (List*)malloc(sizeof(List));
    if (!main_list)
        ERR("malloc");
    
    main_list->head = NULL;
    main_list->size = 0;

    for (int i = 1; i <= k; i++)
        add_to_list(&main_list, i);

    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    if (!args)
    {
        delete_list(&main_list);
        ERR("malloc");
    }

    int quitFlag = 0;

    pthread_mutex_t quitFlagMutex;
    if (pthread_mutex_init(&quitFlagMutex, NULL))
    {
        delete_list(&main_list);
        free(args);
        ERR("pthread_mutex_init");
    }
    
    pthread_mutex_t mainListMutex;
    if (pthread_mutex_init(&mainListMutex, NULL))
    {
        delete_list(&main_list);
        free(args);
        pthread_mutex_destroy(&quitFlagMutex);
        ERR("pthread_mutex_init");
    }

    args->main_list = &main_list;
    args->main_list_mutex = &mainListMutex;
    args->quit_flag = &quitFlag;
    args->quit_flag_mutex = &quitFlagMutex;
    args->mask = &mask;
    
    pthread_t signal_handler_thread;
    if (pthread_create(&signal_handler_thread, NULL, signal_handler_work, (void*)args))
    {
        delete_list(&main_list);
        free(args);
        pthread_mutex_destroy(&quitFlagMutex);
        pthread_mutex_destroy(&mainListMutex);
        ERR("pthread_create");
    }

    
    LItem* p = main_list->head;
    while (1)
    {
        sleep(1);

        pthread_mutex_lock(&quitFlagMutex);
        if (quitFlag == 1)
        {
            if (pthread_join(signal_handler_thread, NULL))
                ERR("pthread_join");
            delete_list(&main_list);
            free(args);
            pthread_mutex_destroy(&mainListMutex);
            pthread_mutex_unlock(&quitFlagMutex);
            pthread_mutex_destroy(&quitFlagMutex);
            break;
        }
        pthread_mutex_unlock(&quitFlagMutex);

        pthread_mutex_lock(&mainListMutex);
        p = main_list->head;
        printf("Current list is: ");
        while (p)
        {
            printf("%d ", p->value);
            p = p->next;
        }
        pthread_mutex_unlock(&mainListMutex);
        printf("\n");
    }
    
    return EXIT_SUCCESS;
}