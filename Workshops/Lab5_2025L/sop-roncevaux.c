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

void attack_enemy (Knight* me, int is_franci, int* write_fds, int franci_count, int saraceni_count,
    int* alive_enemies, int* alive_enemies_number)
{
    if (*alive_enemies_number == 0)
        return;
    
    int random_enemy_index = rand() % *alive_enemies_number;
    int enemy_to_attack = alive_enemies[random_enemy_index];

    uint8_t random_damage = rand() % me->attack + 1;
    if (write(write_fds[enemy_to_attack], &random_damage, 1) == -1)
    {
        if (errno == EPIPE)
        {
            printf("[%d] %s tries to attack, but his enemy is already dead\n", getpid(), me->name);
            alive_enemies[random_enemy_index] = alive_enemies[*alive_enemies_number - 1];
            (*alive_enemies_number)--;
            attack_enemy(me, is_franci, write_fds, franci_count, saraceni_count, 
                alive_enemies, alive_enemies_number);
            return;
        }
        else ERR("write");
    }
        
    
    if (random_damage == 0)
        printf("[%d] %s attacks his enemy, however he deflected (0 damage to %d)\n", getpid(),
         me->name, enemy_to_attack);
    if (random_damage > 0 && random_damage <= 5)
        printf("[%d] %s goes to strike, he hit right and well (%d damage to %d)\n", getpid(), 
        me->name, random_damage, enemy_to_attack);
    if (random_damage >= 6)
        printf("[%d] %s strikes powerful blow, the shield he breaks and inflicts a big wound (%d damage to %d)\n",
             getpid(), me->name, random_damage, enemy_to_attack);
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());
    if (set_handler(SIG_IGN, SIGPIPE) == -1)
        ERR("set_handler");

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

            srand(time(NULL) + getpid());

            int flags = fcntl(read_fds[i], F_GETFL, 0);
            if (flags == -1)
                ERR("fcntl");
            
            if (fcntl(read_fds[i], F_SETFL, flags | O_NONBLOCK) == -1)
                ERR("fcntl");

            Knight* me;
            if (i < f_count)
                me = &francis[i];
            else me = &saracenis[i - f_count];

            int alive_count = (i < f_count) ? s_count : f_count;
            int* alive_enemies = (int*) malloc (sizeof(int) * alive_count);
            if (!alive_enemies)
                ERR("malloc");
            for (int j = 0; j < alive_count; j++)
                alive_enemies[j] = (i < f_count) ? (j + f_count) : j;
            

            while (1)
            {
                int read_index = read_fds[i];
                
                uint8_t damage_received;
                ssize_t bytes_read;

                while ((bytes_read = read(read_index, &damage_received, sizeof(uint8_t))) > 0)
                {
                    me->hp -= damage_received;
                    printf("[%d] %s received %d damage, remaining hp: %d\n", getpid(), 
                        me->name, damage_received, me->hp >= 0 ? me->hp : 0);
                    if (me->hp <= 0)
                       break;
                }
                if (bytes_read == -1 && errno != EAGAIN)
                    ERR("read");
                
                if (me->hp <= 0)
                {
                    printf("[%d] %s dies\n", getpid(), me->name);
                    break;
                }
                
                attack_enemy(me, (i < f_count) ? 1 : 0, write_fds, f_count, s_count, 
                    alive_enemies, &alive_count);
                if (alive_count == 0)
                {
                    printf("[%d] no more enemies, I win! %s are victorious!\n", getpid(), 
                        (i < f_count) ? "Franks" : "Saracens");
                    break;
                }
                int random_sleep = rand() % 9 + 1;
                msleep(random_sleep * 100);
            }

            close(read_fds[i]);
            for (int k = (i < f_count) ? f_count : 0; k < ((i < f_count) ? f_count + s_count : f_count); k++)
            {
                close(write_fds[k]);
            }

            free(francis);
            free(saracenis);
            free(read_fds);
            free(write_fds);
            free(alive_enemies);
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