#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_READERS 5

int shared_data = 0;
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void* reader(void* arg)
{
    int reader_id = (int)(intptr_t)arg;
    for (int i = 0; i < 10; i++)
    {
        pthread_rwlock_rdlock(&rwlock);
        printf("Reader %d: read shared_data = %d\n", reader_id, shared_data);
        sleep(2);
        pthread_rwlock_unlock(&rwlock);
        usleep(1000);
    }

    return NULL;
}

void* writer(void* arg)
{
    for (int i = 0; i < 5; i++)
    {
        pthread_rwlock_wrlock(&rwlock);
        shared_data += 1;
        printf("Writer: updated shared_data to %d\n", shared_data);
        sleep(3);
        pthread_rwlock_unlock(&rwlock);
        usleep(1000);
    }

    return NULL;
}

int main()
{
    pthread_t readers[MAX_READERS];
    pthread_t writer_thread;

    for (int i = 0; i < MAX_READERS; i++)
    {
        if (pthread_create(&readers[i], NULL, reader, (void*)(intptr_t)i))
            ERR("pthread_create");
    }

    if (pthread_create(&writer_thread, NULL, writer, NULL))
        ERR("pthread_create");

    for (int i = 0; i < MAX_READERS; i++)
        pthread_join(readers[i], NULL);

    pthread_join(writer_thread, NULL);

    pthread_rwlock_destroy(&rwlock);
    return EXIT_SUCCESS;
}