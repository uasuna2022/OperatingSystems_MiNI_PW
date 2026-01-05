#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char** argv)
{
    printf("USAGE: %s k n\n\t 1 <= k <= 10\n\t 10 <= n <= 10000\n", argv[0]);
    exit(EXIT_FAILURE);
}

void* pi_estimation (void* thread_args)
{
    int counter = 0;
    int n = *(int*)thread_args;
    unsigned int seed = (unsigned int)pthread_self();

    for (int i = 0; i < n; i++)
    {
        double random_x = ((double)rand_r(&seed) / RAND_MAX) * 2.0 - 1.0;
        double random_y = ((double)rand_r(&seed) / RAND_MAX) * 2.0 - 1.0;
        double distance = sqrt(random_x * random_x + random_y * random_y);
        if (distance <= 1.0)
            counter++;
    }

    printf("[%ld] Thread finished: inside_circle = %d out of %d\n", pthread_self(), counter, n);
    pthread_exit((void*)(intptr_t)counter);
}

int main (int argc, char** argv)
{
    if (argc != 3)
        usage(argc, argv);
    int k = atoi(argv[1]);
    int n = atoi(argv[2]);
    if (k < 1 || k > 10 || n < 10 || n > 1000000)
        usage(argc, argv);

    pthread_t* thread_ids = (pthread_t*)malloc(k * sizeof(pthread_t));
    if (thread_ids == NULL)
        ERR("malloc");
    
    for (int i = 0; i < k; i++)
    {
        if (pthread_create(&thread_ids[i], NULL, pi_estimation, (void*)&n))
            ERR("pthread_create");
    }

    printf("[%ld] All estimation threads created.\n", pthread_self());

    double* output_values = (double*)malloc(k * sizeof(double));
    if (output_values == NULL)
        ERR("malloc");
    
    for (int i = 0; i < k; i++)
    {
        void* thread_result;
        if (pthread_join(thread_ids[i], &thread_result))
            ERR("pthread_join");
        int inside_circle = (intptr_t)thread_result;
        output_values[i] = (4.0 * inside_circle) / n;
        printf("[%ld] Thread %ld: inside_circle = %d, pi_estimate = %f\n", pthread_self(), 
            thread_ids[i], inside_circle, output_values[i]);
    }
    
    double pi_final = 0.0;
    for (int i = 0; i < k; i++)
    {
        pi_final += output_values[i];
    }

    pi_final /= k;
    printf("[%ld] Final pi estimation after %d threads: %f\n", pthread_self(), k, pi_final);
    
    free(thread_ids);
    free(output_values);

    return EXIT_SUCCESS;
}