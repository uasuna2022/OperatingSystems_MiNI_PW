#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int main(int argc, char **argv)
{
    if (chdir("../test"))
        ERR("chdir");

    int fd = open("16102025.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
        ERR("open");

    for (int i = 0; i < 10; i++)
    {
        char buffer[32];
        int n = snprintf(buffer, sizeof buffer, "%d\n", i);  // konwersja int -> tekst z \n
        if (n < 0 || n >= (int)sizeof buffer)
            ERR("snprintf");

        // Zapisz dokładnie n bajtów
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(fd, buffer + off, n - off);
            if (w < 0) ERR("write");
            off += w;
        }

        sleep(1);
    }

    close(fd);
    return EXIT_SUCCESS;
}
