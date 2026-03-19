#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_GRAPH_NODES 32
#define MAX_PATH_LENGTH (2 * MAX_GRAPH_NODES)
#define PATH_MAX 4096

#define FIFO_NAME "/tmp/colony_fifo"

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int ms)
{
    struct timespec tt;
    tt.tv_sec = ms / 1000;
    tt.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

void usage(int argc, char* argv[])
{
    printf("%s graph start dest\n", argv[0]);
    printf("  graph - path to file containing colony graph\n");
    printf("  start - starting node index\n");
    printf("  dest - destination node index\n");
    exit(EXIT_FAILURE);
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
    int neighbors[MAX_GRAPH_NODES];
    int number_of_neighbors;
} Vertex;

typedef struct {
    int id;
    int path[MAX_PATH_LENGTH];
    int current_length;
} Ant;

int parse_file(Vertex** G, FILE* f)
{
    int N;
    errno = 0;
    if (fscanf(f, "%d\n", &N) <= 0)
        ERR("fscanf");
    
    *G = (Vertex*) malloc (sizeof(Vertex) * N);
    if (!(*G))
        ERR("malloc");

    for (int i = 0; i < N; i++)
        (*G)[i].number_of_neighbors = 0;
    
    
    int u, v;
    int fields_read;
    while ((fields_read = fscanf(f, "%d %d\n", &u, &v)) == 2)
    {
        int cur_neighbors = (*G)[u].number_of_neighbors;
        (*G)[u].neighbors[cur_neighbors] = v;
        (*G)[u].number_of_neighbors++;
    }
    if (fields_read == -1 && errno != 0)
        ERR("fscanf");
    /* if (ferror(f))
        ERR("fscanf"); */
    if (fields_read != EOF && fields_read != 2)
        ERR("error in file format");

    return N;
}

void close_pipes(int number, int* read_fds, int* write_fds, Vertex* G, int N)
{
    Vertex me = G[number];
    for (int i = 0; i < N; i++)
    {
        if (number == i)
            continue;
        else close(read_fds[i]);
    }
    int* arr = (int*)malloc(sizeof(int) * N);
    for (int i = 0; i < N; i++)
        arr[i] = 0;
    for (int i = 0; i < me.number_of_neighbors; i++)
        arr[me.neighbors[i]] = 1;

    for (int i = 0; i < N; i++)
        if (arr[i] == 0)
            close(write_fds[i]);
    
    free(arr);
}

volatile sig_atomic_t sigint_received = 0;
void sigint_handler(int sig) { sigint_received = 1; }

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argc, argv);

    int start_vertex = atoi(argv[2]);
    int dest_vertex = atoi(argv[3]);

    sigset_t mask, oldmask;
    if (sigemptyset(&mask))
        ERR("sigemptyset");
    if (sigaddset(&mask, SIGINT))
        ERR("sigaddset");
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask))
        ERR("sigprocmask");
    
    FILE* f = fopen(argv[1], "r");
    if (!f) 
        ERR("fopen");
    
    Vertex* G;
    int N = parse_file(&G, f);
    fclose(f);

    int* read_fds = (int*) malloc (sizeof(int) * N);
    if (!read_fds)
        ERR("malloc");
    int* write_fds = (int*) malloc (sizeof(int) * N);
    if (!write_fds) 
        ERR("malloc");

    for (int i = 0; i < N; i++)
    {
        int fd[2];
        if (pipe(fd))
            ERR("pipe");
        read_fds[i] = fd[0];
        write_fds[i] = fd[1];
    }

    for (int i = 0; i < N; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
            ERR("fork");
        if (pid == 0)
        {
            srand(getpid());
            printf("[%d] Process started\n", getpid());
            if (set_handler(sigint_handler, SIGINT))
                ERR("set_handler");
            Vertex* me = &G[i];

            close_pipes(i, read_fds, write_fds, G, N);
            
            ssize_t bytes_read, bytes_wrote;
            Ant received_ant;
            while ((bytes_read = read(read_fds[i], &received_ant, sizeof(Ant))) > 0)
            {
                if (bytes_read == -1 && errno == EINTR)
                {
                    if (sigint_received)
                    {
                        printf("[%d] SIGINT received, terminating...\n", getpid());
                        break;
                    }
                    else 
                    {
                        printf("[%d] Interrupted by some signal\n", getpid());
                        continue;
                    }
                }
                if (bytes_read == -1 && errno != EINTR)
                    ERR("read");

                received_ant.path[received_ant.current_length] = i;
                received_ant.current_length++;

                if (i == dest_vertex)
                {
                    printf("[%d] Ant %d: found food\n", getpid(), received_ant.id);
                    continue;
                }
                if (received_ant.current_length == MAX_PATH_LENGTH || me->number_of_neighbors == 0)
                {
                    printf("[%d] Ant %d: got lost\n", getpid(), received_ant.id);
                    continue;
                }

                int random_neighbour = me->neighbors[rand() % me->number_of_neighbors];
                if ((bytes_wrote = write(write_fds[random_neighbour], &received_ant, sizeof(Ant))) != sizeof(Ant))
                {
                    if (bytes_wrote > 0)
                        ERR("partial write");
                    if (bytes_wrote == -1 && errno == EINTR)
                    {
                        if (sigint_received)
                        {
                            printf("[%d] SIGINT received, terminating...\n", getpid());
                            break;
                        }
                        else 
                        {
                            printf("[%d] Interrupted by some signal\n", getpid());
                            continue;
                        }
                    }
                    if (bytes_wrote == -1 && errno != EINTR)
                        ERR("write");
                }

                msleep(100);
            }


            free(G);
            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < N; i++)
    {
        if (i != start_vertex)
            if (close(write_fds[i]))
                ERR("close");
        if (close(read_fds[i]))
            ERR("close");
    }

    int id = 0;
    while (1)
    {
        msleep(1000);
        Ant* ant = (Ant*)malloc(sizeof(Ant));
        if (!ant)
            ERR("malloc");
        ant->id = id;
        ant->current_length = 0;
        
        ssize_t bytes_wrote;
        if ((bytes_wrote = write(write_fds[start_vertex], ant, sizeof(Ant))) == -1)
            ERR("write");
        if (bytes_wrote != sizeof(Ant))
            ERR("partial write");

        id++;
    }
    
    while (wait(NULL) > 0) {}
    exit(EXIT_SUCCESS);
}