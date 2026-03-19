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

// --- STRUKTURY ---
typedef struct {
    int neighbors[MAX_GRAPH_NODES];
    int number_of_neighbors;
} Vertex;

typedef struct {
    int id;
    int path[MAX_PATH_LENGTH];
    int current_length;
} Ant;

// --- FUNKCJE POMOCNICZE ---
volatile sig_atomic_t sigint_received = 0;
void sigint_handler(int sig) { sigint_received = 1; }

int set_handler(void (*f)(int), int sig) {
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1) return -1;
    return 0;
}

void msleep(int ms) {
    struct timespec tt = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&tt, &tt) == -1 && errno == EINTR);
}

// --- LOGIKA GRAFU I CZYSZCZENIA ---
int parse_file(Vertex** G, FILE* f) {
    int N;
    if (fscanf(f, "%d\n", &N) <= 0) ERR("fscanf");
    *G = (Vertex*)malloc(sizeof(Vertex) * N);
    for (int i = 0; i < N; i++) (*G)[i].number_of_neighbors = 0;
    int u, v;
    while (fscanf(f, "%d %d\n", &u, &v) == 2) {
        (*G)[u].neighbors[(*G)[u].number_of_neighbors++] = v;
    }
    return N;
}

void close_pipes(int my_id, int* read_fds, int* write_fds, Vertex* G, int N) {
    for (int i = 0; i < N; i++) {
        if (i != my_id) close(read_fds[i]);
        int is_neighbor = 0;
        for (int j = 0; j < G[my_id].number_of_neighbors; j++)
            if (G[my_id].neighbors[j] == i) is_neighbor = 1;
        if (!is_neighbor) close(write_fds[i]);
    }
}

// --- MAIN ---
int main(int argc, char* argv[]) {
    if (argc != 4) { printf("%s graph start dest\n", argv[0]); exit(EXIT_FAILURE); }
    int start_node = atoi(argv[2]);
    int dest_node = atoi(argv[3]);

    // 1. Blokowanie sygnałów i przygotowanie FIFO
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    set_handler(sigint_handler, SIGINT);

    mkfifo(FIFO_NAME, 0666);

    FILE* f = fopen(argv[1], "r");
    Vertex* G;
    int N = parse_file(&G, f);
    fclose(f);

    int read_fds[MAX_GRAPH_NODES], write_fds[MAX_GRAPH_NODES];
    for (int i = 0; i < N; i++) {
        int fd[2];
        if (pipe(fd)) ERR("pipe");
        read_fds[i] = fd[0];
        write_fds[i] = fd[1];
    }

    // 2. TWORZENIE WĘZŁÓW
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            srand(getpid());
            close_pipes(i, read_fds, write_fds, G, N);
            sigprocmask(SIG_UNBLOCK, &mask, NULL); // Potomek reaguje na SIGINT

            Ant ant;
            while (!sigint_received) {
                ssize_t res = read(read_fds[i], &ant, sizeof(Ant));
                if (res <= 0) { if (errno == EINTR) continue; break; }

                ant.path[ant.current_length++] = i;

                if (i == dest_node) {
                    int fd = open(FIFO_NAME, O_WRONLY);
                    if (fd != -1) { write(fd, &ant, sizeof(Ant)); close(fd); }
                } else if (G[i].number_of_neighbors > 0 && ant.current_length < MAX_PATH_LENGTH) {
                    msleep(100);
                    int next = G[i].neighbors[rand() % G[i].number_of_neighbors];
                    write(write_fds[next], &ant, sizeof(Ant));
                }
            }
            free(G); exit(EXIT_SUCCESS);
        }
    }

    // 3. PROCES GŁÓWNY - PĘTLA SYMULACJI
    for (int i = 0; i < N; i++) { 
        close(read_fds[i]); 
        if (i != start_node) close(write_fds[i]); 
    }

    int fifo_fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
    int ant_id = 0;
    time_t last_ant_time = 0;

    while (!sigint_received) {
        // Czytanie z FIFO (nieblokujące)
        Ant finished_ant;
        if (read(fifo_fd, &finished_ant, sizeof(Ant)) == sizeof(Ant)) {
            printf("Ant id: found food. Path: ");
            for (int k = 0; k < finished_ant.current_length; k++) 
                printf("%d ", finished_ant.path[k]);
            printf("\n");
        }

        // Wysyłanie nowej mrówki co 1000ms
        time_t now = time(NULL);
        if (now > last_ant_time) {
            Ant new_ant = {ant_id++, {0}, 0};
            write(write_fds[start_node], &new_ant, sizeof(Ant));
            last_ant_time = now;
        }
        msleep(50); // Krótki sen, żeby nie obciążać procesora w pętli nieblokującej
    }

    // 4. CZYSZCZENIE
    printf("\nClosing colony...\n");
    kill(0, SIGINT);
    while (wait(NULL) > 0);
    close(fifo_fd);
    unlink(FIFO_NAME);
    free(G);
    return 0;
}