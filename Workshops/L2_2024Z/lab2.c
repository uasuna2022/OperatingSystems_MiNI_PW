#define _GNU_SOURCE 
#include <errno.h> 
#include <fcntl.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#include <sys/wait.h> 
#include <time.h> 
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

ssize_t bulk_read(int fd, char* buf, size_t count) 
{ 
    ssize_t c; 
    ssize_t len = 0; 
    do 
    { 
        c = TEMP_FAILURE_RETRY(read(fd, buf, count)); 
        if (c < 0) 
            return c; 
        if (c == 0) 
            return len; // EOF 
        buf += c; 
        len += c; 
        count -= c; 
    }
    while (count > 0); 
    
    return len; 
}

ssize_t bulk_write(int fd, char* buf, size_t count) 
{ 
    ssize_t c; 
    ssize_t len = 0; 
    do 
    { 
        c = TEMP_FAILURE_RETRY(write(fd, buf, count)); 
        if (c < 0) 
            return c; 
        buf += c; 
        len += c; 
        count -= c; 
    } 
    while (count > 0); 

    return len; 
}

void usage(int argc, char* argv[]) 
{ 
    printf("%s p k \n", argv[0]); 
    printf("\tp - path to file to be encrypted\n"); 
    printf("\t0 < k < 8 - number of child processes\n"); 
    exit(EXIT_FAILURE); 
}

void sethandler(void (*f)(int), int sigNo) 
{ 
    struct sigaction act; 
    memset(&act, 0, sizeof(struct sigaction)); 
    act.sa_handler = f; 
    if (-1 == sigaction(sigNo, &act, NULL)) 
        ERR("sigaction"); 
}

ssize_t child_work(int fd, int child_number, ssize_t block_size, char* buf)
{
    ssize_t bytes_read = -1;
    int start_pos = child_number * block_size;
    lseek(fd, start_pos, SEEK_SET);
    bytes_read = bulk_read(fd, buf, block_size);
    buf[block_size] = '\0';
    return bytes_read;
}

volatile sig_atomic_t sig_usr1 = 0;
void sigusr1_handler(int sigNo) { sig_usr1 = 1; }

int main(int argc, char** argv)
{
    if (argc != 3)
        usage(argc, argv);
    
    int k = atoi(argv[2]);
    if (k <= 0 || k >= 8) 
        usage(argc, argv);
    
    char* path = argv[1];
    int fd = open(path, O_RDWR | O_NONBLOCK | O_SYNC);
    if (fd < 0)
        ERR("open");
    
    struct stat st;
    fstat(fd, &st);
    ssize_t file_size = st.st_size;
    ssize_t block_size = file_size / k + 1;

    pid_t* pids = (pid_t*)malloc(sizeof(pid_t) * k);

    printf("[%d] Hi, I'm a main parent process, my pid is %d\n", getpid(), getpid());
    for (int i = 1; i <= k; i++)
    {
        pid_t pid = fork();
        switch(pid)
        {
            case -1:
                ERR("fork");
            case 0:
                printf("[%d] Hi, I'm a %dth child\n", getpid(), i);

                char* buf = (char*)malloc(sizeof(char) * (block_size + 1));
                ssize_t bytes_read = child_work(fd, i - 1, block_size, buf);

                sigset_t mask, oldmask;
                sigemptyset(&mask);
                sigaddset(&mask, SIGUSR1);
                sigprocmask(SIG_BLOCK, &mask, &oldmask);
                sethandler(sigusr1_handler, SIGUSR1);

                while (sig_usr1 == 0) 
                {
                    sigsuspend(&oldmask);
                }

                printf("[%d] Got a SIGUSR1 signal. Terminating...\n", getpid());

                // logika przetwarzania odcinku pliku

                free(buf);
                exit(EXIT_SUCCESS);
            default:
                pids[i - 1] = pid;
                break;
        }
    }

    for (int i = 0; i < k; i++)
    {
        kill(pids[i], SIGUSR1);
    }
    while (wait(NULL) > 0) { /* waiting for all children to finish */}
    printf("[%d] All children finished their work.\n", getpid());

    close(fd);
    exit(EXIT_SUCCESS);
}
