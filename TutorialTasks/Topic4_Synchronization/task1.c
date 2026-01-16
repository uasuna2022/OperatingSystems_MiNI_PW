#define _GNU_SOURCE
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_ALARMS 5
#define MAX_TIME 120
#define MAX_BUF_LENGTH 10

typedef struct {
    sem_t* semaphore;
    int seconds;
} thread_str;

volatile sig_atomic_t work = 1;
void sigint_handler(int sigNo) { work = 0; }
void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void* thread_job (void* arg)
{
    thread_str* args = (thread_str*)arg;
    int seconds = args->seconds;
    sem_t* semaphore = args->semaphore;

    free(args);

    printf("[%ld] New alarm set (%d seconds)\n", pthread_self(), seconds);
    sleep(seconds);
    printf("[%ld] Wake up!\n", pthread_self());
    if (sem_post(semaphore))
        ERR("sem_post");

    return NULL;
}

int main (int argc, char** argv)
{
    sem_t* semaphore = (sem_t*) malloc (sizeof(sem_t));
    if (!semaphore)
        ERR("malloc");
    
    if (sem_init(semaphore, 0, MAX_ALARMS))
    {
        free(semaphore);
        ERR("sem_init");
    }

    sethandler(sigint_handler, SIGINT);

    while (work)
    {
        char buf[MAX_BUF_LENGTH];
        puts("Enter a number of seconds for a new alarm:");
        if (!fgets(buf, MAX_BUF_LENGTH, stdin))
            break;
        int seconds = atoi(buf);
        if (buf[0] == 's' && buf[1] == 't' && buf[2] == 'a' && buf[3] == 't')
        {
            int sval;
            if (sem_getvalue(semaphore, &sval))
                ERR("sem_getvalue");
            printf("Running alarms: %d\n", MAX_ALARMS - sval);
            continue;
        }
            
        if (seconds <= 0 || seconds > MAX_TIME)
        {
            puts("Invalid time value (enter a number between 1 and 120)");
            continue;
        }

        if (TEMP_FAILURE_RETRY(sem_trywait(semaphore)) == -1)
        {
            if (errno == EAGAIN)
            {
                puts("No free slots for new alarms. Try again later.");
                continue;
            }
            if (errno == EINTR)
                continue;
            ERR("sem_trywait");
        }

        thread_str* args = (thread_str*) malloc (sizeof(thread_str));
        if (!args)
        {
            free(semaphore);
            ERR("malloc");
        }

        args->semaphore = semaphore;
        args->seconds = seconds;

        pthread_t tid;
        pthread_attr_t attr;
        if (pthread_attr_init(&attr))
            ERR("pthread_attr_init");
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
            ERR("pthread_attr_setdetachstate");
        if (pthread_create(&tid, &attr, thread_job, (void*)args))
            ERR("pthread_create");
        if (pthread_attr_destroy(&attr))
            ERR("pthread_attr_destroy");
    }

    while (1)
    {
        int sval;
        if (sem_getvalue(semaphore, &sval))
            ERR("sem_getvalue");
        if (sval == MAX_ALARMS)
            break;
        printf("Waiting for %d alarms to finish...\n", MAX_ALARMS - sval);
        sleep(2);
    }

    if (sem_destroy(semaphore))
        ERR("sem_destroy");

    free(semaphore);
    return EXIT_SUCCESS;
}