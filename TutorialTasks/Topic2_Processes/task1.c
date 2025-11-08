#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void usage(char* path)
{
    fprintf(stderr, "Usage: %s N>0 (number of child processes)\n", path);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc != 2)
        usage(argv[0]);
    if (atoi(argv[1]) <= 0)
        usage(argv[0]);

    int N = atoi(argv[1]);
    for (int i = 1; i <= N; i++)
    {
        int pid = fork();
        switch(pid)
        {
            case -1:
                ERR("fork");
            case 0:
                fprintf(stdout, "[%d] Hello, I'm the %dth child process! My parent's pid is %d.\n", 
                    getpid(), i, getppid());
                srand(time(NULL) ^ (getpid()<<16));
                int sleep_seconds = 5 + rand() % 6;
                fprintf(stdout, "[%d] I'm sleeping for %d seconds...\n", getpid(), sleep_seconds);
                sleep(sleep_seconds);
                fprintf(stdout, "[%d] Dream finished. I'm being terminated now. Bye! :((\n", getpid());
                exit(0);
            default:
                // fprintf(stdout, "[%d] Hello, I'm a parent process.\n", getpid());
                break;
        }
    }
    
    while (N > 0)
    {   
        while (1)
        {
            int status;
            pid_t childPID = waitpid(-1, &status, WNOHANG);
            if (childPID > 0)
            {
                N--;
                if (WIFEXITED(status))
                {
                    fprintf(stdout, "[%d] Child process %d terminated normally with exit code %d.\n", 
                        getpid(), childPID, WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status))
                {
                    fprintf(stdout, "[%d] Child process %d was terminated by signal %d.\n", 
                        getpid(), childPID, WTERMSIG(status));
                }
                fflush(stdout);
                continue;
            }

            if (childPID == 0 || errno == ECHILD)
                break;

            if (childPID == -1 && errno != ECHILD)
            {
                ERR("waitpid");
            }
        } 

        sleep(3);
        fprintf(stdout, "[%d] I'm a parent process. %d child processes are still working.\n", 
            getpid(), N);
    }

    printf("[%d] All children finished. Bye!\n", getpid());
    return EXIT_SUCCESS;
}