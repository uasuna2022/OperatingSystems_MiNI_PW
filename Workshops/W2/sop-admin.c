#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define FILE_MAX_SIZE 512

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
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void usage(int argc, char* argv[])
{
    printf("%s p h\n", argv[0]);
    printf("\tp - path to directory describing the structure of the Austro-Hungarian office in Prague.\n");
    printf("\th - Name of the highest administrator.\n");
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

// 'volatile' keyword means, that the value of the program may be changed
// without any action being taken by the code compiler here.
// 'sig_atomic_t' - data type that guarantees us, that variable initialization
// will be finished with full atomicity (usual 'int' may be interrupted in the middle of the program).
volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t sig_usr2 = 0;
volatile sig_atomic_t sig_int = 0;

// Handlers are just raising the appropriate flags. They can't print anything, because it's unsafe.
void user1_handler(int sig) { sig_usr1 = 1; }
void user2_handler(int sig) { sig_usr2 = 1; }
void int_handler(int sig) { sig_int = 1; }

void analize_file(char* name, sigset_t* oldMask, int is_admin)
{
    printf("My name is %s and my PID is %d\n", name, getpid());
    
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        ERR("open");
    
    char buf[FILE_MAX_SIZE + 1] = {0};
    ssize_t nBytes = bulk_read(fd, buf, FILE_MAX_SIZE);
    if (nBytes < 0)
        ERR("bulk_read");
    buf[nBytes] = '\0';

    close(fd);

    pid_t children_pids[2] = {0};
    
    char* child_name = strtok(buf, "\n");
    int child_count = 0;
    while (child_name)
    {
        if (strcmp(child_name, "-") == 0)
        {
            child_name = strtok(NULL, "\n");
            continue;
        }

        printf("%s is inspecting a subordinate named %s\n", name, child_name);
        pid_t child_pid = fork();
        switch (child_pid)
        {
            case -1:
                ERR("fork");
            case 0:
                analize_file(child_name, oldMask, 0);
                exit(EXIT_SUCCESS);
            default:
                children_pids[child_count++] = child_pid;
                break;
        }
        
        child_name = strtok(NULL, "\n");
    }
    
    printf("%s has inspected all subordinates\n", name);
    printf("%s is waiting for documents...\n", name);
    srand(getpid());
    while (sigsuspend(&oldMask))
    {
        if (sig_usr2 == 1)
        {
            sig_usr2 = 0;
            printf("%s received SIGUSR2\n", name);
            switch (rand() % 3)
            {
                case 0:
                case 1:
                    if (is_admin)
                    {
                        printf("%s received a document. Ignoring. XDDD\n", name);
                    }
                    else 
                    {
                        printf("%s received a document. Passing it to the superintendent...\n", name);
                        kill(getppid(), 12);
                    }
                    break;
                case 2:
                    printf("%s received a document. Sending to the archive XD\n", name);
                default:
                    break;
            }
        }

        if (sig_int == 1)
        {
            sig_int = 0;
            printf("%s ending the day.\n", name);
            for (int i = 0; i < child_count; i++)
            {
                kill(children_pids[i], SIGINT);
            }
            break;
        }
    }

    printf("%s is waiting for all the subordinates to finish...\n", name);
    while (wait(NULL) > 0) {}
    printf("%s is leaving an office!\n", name);
    return;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    // Initializing 2 sigsets: for current signal mask and for the new one.
    sigset_t mask, oldMask;
    sigemptyset(&mask);
    sigemptyset(&oldMask);

    // Adding 3 signals to the new mask (SIGUSR1, SIGUSR2 and SIGINT).
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);

    // This function adds all signals from sigset 'mask' to the current process mask and blocks all of them.
    // This function is atomic, after it finishes its work, we have an old process mask state inside oldMask variable.
    sigprocmask(SIG_BLOCK, &mask, &oldMask);

    // Setting handlers to define the behavior after getting an appropriate signal.
    sethandler(user1_handler, SIGUSR1);
    sethandler(user2_handler, SIGUSR2);
    sethandler(int_handler, SIGINT);

    printf("Waiting for SIGUSR1...\n");
    printf("PID: %d\n", getpid());

    // sig_usr1 will be false until we handle a signal SIGUSR1
    // sigsuspend() function does the following:
    // - changes the process mask to oldMask for a moment (in our logic all the signals are becoming unblocked);
    // - process starts to "sleep" (doesn't use the CPU) and waits for the unblocked signal;
    // - when signal is handled, the original process mask is being returned, function finishes its job
    while (!sig_usr1 && sigsuspend(&oldMask)) {/* waiting for the signal */}

    printf("SIGUSR1 received!\n");
    sig_usr1 = 0;
    sig_usr2 = 0;
    sig_int = 0;

    if (chdir(argv[1]) < 0)
        ERR("chdir");

    analize_file(argv[2], &oldMask, 1);
    return EXIT_SUCCESS;
}
