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

int main(int argc, char* argv[])
{
    srand(time(NULL));
    // printf("Opened descriptors: %d\n", count_descriptors());

    FILE* franci_file = fopen("franci.txt", "r");
    if (!franci_file)
    {
        printf("Franks haven't arrived on the battlefield!");
        ERR("fopen");
    }

    FILE* saraceni_file = fopen("saraceni.txt", "r");
    if (!saraceni_file)
    {
        printf("Saracens haven't arrived on the battlefield!");
        ERR("fopen");
    }

    Knight* francis, *saracenis;

    int f_count = parse_file(&francis, franci_file);
    int s_count = parse_file(&saracenis, saraceni_file);



    free(francis);
    free(saracenis);
    return EXIT_SUCCESS;
}