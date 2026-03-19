#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_KNIGHT_NAME_LENGHT 20

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

typedef struct {
    char name[MAX_KNIGHT_NAME_LENGHT + 1];
    int hp;
    int attack;
} Knight;

int parse_file(Knight** knights, FILE* f)
{
    int count;
    if (fscanf(f, "%d\n", &count) <= 0)
        ERR("fscanf");
    
    *knights = (Knight*)malloc(sizeof(Knight) * count);
    if (!(*knights))
        ERR("malloc");
    
    for (int i = 0; i < count; i++)
    {
        if (fscanf(f, "%20s %d %d\n", (*knights)[i].name, &((*knights)[i].hp), &((*knights)[i].attack)) != 3)
            ERR("fscanf");
    }
    
    return count;
}

void close_pipes(int number, int* write_fds, int* read_fds, int is_franci, int franci_count, int saraceni_count)
{
    for (int i = 0; i < franci_count + saraceni_count; i++)
    {
        if (i == number)
            continue;
        else close(read_fds[i]);
    }

    if (is_franci == 1)
    {
        for (int i = 0; i < franci_count; i++)
            close(write_fds[i]);
    }
    else 
    {
        for (int i = 0; i < saraceni_count; i++)
            close(write_fds[i + franci_count]);
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());

    FILE* franci_file = fopen("franci.txt", "r");
    if (!franci_file)
    {
        printf("Franks haven't arrived on the battlefield!\n");
        exit(EXIT_FAILURE);
    }

    FILE* saraceni_file = fopen("saraceni.txt", "r");
    if (!saraceni_file)
    {
        printf("Saracens haven't arrived on the battlefield!\n");
        exit(EXIT_FAILURE);
    }

    Knight* francis, *saracenis;

    int f_count = parse_file(&francis, franci_file);
    int s_count = parse_file(&saracenis, saraceni_file);

    fclose(franci_file);
    fclose(saraceni_file);

    int* read_fds = (int*) malloc (sizeof(int) * (f_count + s_count));
    if (!read_fds)
        ERR("malloc");
    int* write_fds = (int*) malloc (sizeof(int) * (f_count + s_count));
    if (!write_fds)
        ERR("malloc");

    for (int i = 0; i < s_count + f_count; i++)
    {
        int fd[2];
        if (pipe(fd))
            ERR("pipe");
        read_fds[i] = fd[0];
        write_fds[i] = fd[1];
    }

    for (int i = 0; i < f_count + s_count; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        if (pid == 0)
        {
            if (i < f_count) close_pipes(i, write_fds, read_fds, 1, f_count, s_count);
            else close_pipes(i, write_fds, read_fds, 0, f_count, s_count);

            if (i < f_count)
                printf("[%d] I'm french! ", getpid());
            else
                printf("[%d] I'm sarrasin! ", getpid());

            printf("Opened descriptors: %d\n", count_descriptors());

            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < f_count + s_count; i++)
    {
        close(read_fds[i]);
        close(write_fds[i]);
    }

    while (wait(NULL) > 0) {}

    free(francis);
    free(saracenis);
    free(read_fds);
    free(write_fds);
    return EXIT_SUCCESS;
}