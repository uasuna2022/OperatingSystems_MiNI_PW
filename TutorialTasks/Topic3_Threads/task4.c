#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define ELAPSED(start, end) ((end).tv_sec - (start).tv_sec) + (((end).tv_nsec - (start).tv_nsec) * 1.0e-9)

typedef struct timespec timespec_t;

typedef struct {
    bool* kicked;
    pthread_t* student_ids;
    int count; 
} students_array;

typedef struct {
    int* active_students_counters;
    pthread_mutex_t* mutexes;
    int years_count;
} year_counters;

typedef struct {
    year_counters* data;
    int year_index;
} cleanup_args;

void usage (int argc, char** argv)
{
    printf("USAGE: %s n\n\t1 <= n <= 100\n", argv[0]);
    exit(EXIT_FAILURE);
}

void decrement_counter(void* args)
{
    cleanup_args* arguments = args;
    pthread_mutex_lock(&(arguments->data->mutexes[arguments->year_index]));
    arguments->data->active_students_counters[arguments->year_index]--;
    pthread_mutex_unlock(&(arguments->data->mutexes[arguments->year_index]));
}

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

void* student_life (void* arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    cleanup_args c_args;
    year_counters* years = (year_counters*)arg;
    c_args.data = years;
    for (int year = 0; year < years->years_count - 1; year++)
    {
        pthread_mutex_lock(&years->mutexes[year]);
        years->active_students_counters[year]++;
        pthread_mutex_unlock(&years->mutexes[year]);

        c_args.year_index = year;

        pthread_cleanup_push(decrement_counter, (void*)&c_args);
        msleep(1000);
        pthread_cleanup_pop(1);
    }

    pthread_mutex_lock(&years->mutexes[3]);
    years->active_students_counters[3]++;
    pthread_mutex_unlock(&years->mutexes[3]);

    return NULL;
}

void kick_student(students_array* students)
{
    int index;
    do {
        index = rand() % students->count;
    } while (students->kicked[index]);

    pthread_cancel(students->student_ids[index]);
    students->kicked[index] = true;
}

int main (int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    int n = atoi(argv[1]);
    if (n < 1 || n > 100) 
        usage(argc, argv);
    
    students_array* students = (students_array*)malloc(sizeof(students_array));
    if (!students)
        ERR("malloc");
    
    students->count = n;
    students->kicked = (bool*)calloc(n, sizeof(bool));
    if (!students->kicked)
    {
        free(students);
        ERR("malloc");
    }
    students->student_ids = (pthread_t*)malloc(sizeof(pthread_t) * n);
    if (!students->student_ids)
    {
        free(students->kicked);
        free(students);
        ERR("malloc");
    }

    year_counters* years = (year_counters*)malloc(sizeof(year_counters));
    if (!years)
    {
        free(students->kicked);
        free(students->student_ids);
        free(students);
        ERR("malloc");
    }

    years->years_count = 4;
    years->mutexes = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * 4);
    if (!years->mutexes)
    {
        free(students->kicked);
        free(students->student_ids);
        free(students);
        free(years);
        ERR("malloc");
    }

    for (int i = 0; i < 4; i++)
    {
        if (pthread_mutex_init(&years->mutexes[i], NULL))
        {
            free(students->kicked);
            free(students->student_ids);
            free(students);
            free(years->mutexes);
            free(years);
            ERR("pthread_mutex_init");
        }
    }

    years->active_students_counters = (int*)calloc(4, sizeof(int));
    if (!years->active_students_counters)
    {
        free(students->kicked);
        free(students->student_ids);
        free(students);
        for (int i = 0; i < 4; i++)
            pthread_mutex_destroy(&years->mutexes[i]);
        free(years->mutexes);
        free(years);
        ERR("calloc");
    }

    for (int i = 0; i < n; i++)
    {
        if (pthread_create(&students->student_ids[i], NULL, student_life, (void*)years))
        {
            free(students->kicked);
            free(students->student_ids);
            free(students);
            for (int i = 0; i < 4; i++)
                pthread_mutex_destroy(&years->mutexes[i]);
            free(years->mutexes);
            free(years->active_students_counters);
            free(years);
            ERR("calloc");
        }
    }

    timespec_t start, current;
    if (clock_gettime(CLOCK_REALTIME, &start))
        ERR("clock_gettime");

    int currently_kicked = 0;
    
    do {
        msleep(rand() % 201 + 100);
        if (clock_gettime(CLOCK_REALTIME, &current))
            ERR("clock_gettime");
        if (currently_kicked < students->count)
        {
            kick_student(students);
            currently_kicked++;
        }
    } while (ELAPSED(start, current) < 4.0);

    int kicked_total = 0;

    for (int i = 0; i < students->count; i++)
    {   
        void* retval;
        if (pthread_join(students->student_ids[i], &retval))
            ERR("Failed to join with a student thread!");
        if (retval == PTHREAD_CANCELED)
            kicked_total++;
    }
    printf(" First year: %d\n", years->active_students_counters[0]);
    printf("Second year: %d\n", years->active_students_counters[1]);
    printf(" Third year: %d\n", years->active_students_counters[2]);
    printf("  Engineers: %d\n", years->active_students_counters[3]);
    printf("Kicked students: %d out of %d\n", kicked_total, students->count);
    free(students->kicked);
    free(students->student_ids);
    exit(EXIT_SUCCESS);

    return EXIT_SUCCESS;
}