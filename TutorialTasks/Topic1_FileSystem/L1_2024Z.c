#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat");
        return EXIT_FAILURE;
    }

    char cmd[64];
    if (scanf("%63s", cmd) != 1) {
        fprintf(stderr, "No command provided on standard input\n");
        return EXIT_FAILURE;
    }

    if (strcmp(cmd, "groups") == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: 'groups' requires a directory argument\n");
            return EXIT_FAILURE;
        }
        int rc = get_groups_count(path);
        if (rc != 0) fprintf(stderr, "get_groups_count returned %d\n", rc);
        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (strcmp(cmd, "process") == 0) {
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Error: 'process' requires a file argument\n");
            return EXIT_FAILURE;
        }
        int groupno;
        if (scanf("%d", &groupno) != 1) {
            fprintf(stderr, "Invalid or missing group number\n");
            return EXIT_FAILURE;
        }
        int rc = process_file(path, groupno);
        if (rc != 0) fprintf(stderr, "process_file returned %d\n", rc);
        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (strcmp(cmd, "batch") == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: 'batch' requires a directory argument\n");
            return EXIT_FAILURE;
        }
        int rc = batch_process(path);
        if (rc != 0) fprintf(stderr, "batch_process returned %d\n", rc);
        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return EXIT_FAILURE;
    }
}