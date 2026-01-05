#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void get_time_string(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", timeinfo);
}

void* thread_routine (void* thread_args)
{
    char time_str[9];
    sleep(1);
    get_time_string(time_str, sizeof(time_str));
    printf("[%s] Thread started... [%ld]\n", time_str, pthread_self());
    int seconds = (int)thread_args;
    
    for (int i = 0; i < 3; i++)
    {
        get_time_string(time_str, sizeof(time_str));
        printf("[%s] Hello from a thread! [%ld]\n", time_str, pthread_self());
        sleep(seconds);
    }

    int value = 5 + rand() % 6;
    get_time_string(time_str, sizeof(time_str));
    printf("[%s] Thread finished, returning %d [%ld]\n", time_str, value, pthread_self());
    pthread_exit((void*)value);
}

int main(int argc, char** argv)
{
    char time_str[9];
    srand(getpid());
    char* array = (char*)malloc(10 * sizeof(char));
    array[0] = 'a';
    array[1] = 'b';
    array[2] = 'c';
    array[3] = '\0';
    get_time_string(time_str, sizeof(time_str));
    printf("[%s] Array pointer: %p, string: %s [%ld]\n", time_str, array, array, pthread_self());
    
    /*
    int local_variable = 42;
    printf("[%ld] Local array pointer: %p, string: %d\n", pthread_self(), &local_variable, local_variable);
    */
    get_time_string(time_str, sizeof(time_str));
    printf("[%s] Hello from a main thread! [%ld]\n", time_str, pthread_self());
    pthread_t thread_ids[2];
    for (int i = 0; i < 2; i++)
    {
        if (pthread_create(&thread_ids[i], NULL, thread_routine, 4 - i))
            ERR("pthread_create");
    }
    
    get_time_string(time_str, sizeof(time_str));
    printf("[%s] Waiting for both threads to finish... [%ld]\n", time_str, pthread_self());

    for (int i = 0; i < 2; i++)
    {
        if (pthread_join(thread_ids[i], NULL))
            ERR("pthread_join");
        printf("Joined with thread %ld\n", thread_ids[i]);
    }
    printf("I've joined everything!\n");

    free(array);
    pthread_exit(NULL);
}