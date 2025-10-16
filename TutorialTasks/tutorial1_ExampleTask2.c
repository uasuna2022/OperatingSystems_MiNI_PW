#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage()
{
    char* msg = "USAGE: ./progfilename -p dir1 -p dir2 -d 2 -p dir3 -d 3 -e txt -o ...";
    write(2, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void basicDirectionScan(char* path)
{
    DIR* dir = opendir(path);
    if (!dir && errno == ENOENT)
        ERR("directory does not exist");
    if (!dir && errno != ENOENT)
        ERR("opendir");

    char dirPath[200];
    if (!getcwd(dirPath, 200))
        ERR("getcwd");
    
    struct dirent* entry;
    struct stat st;
    printf("path: %s\n", dirPath);
    while ((entry = readdir(dir)) != NULL)
    {
        if (lstat(entry->d_name, &st) < 0)
            ERR("lstat");
        if (S_ISREG(st.st_mode))
        {
            printf("\t%s\t", entry->d_name);
            printf("%ld\n", st.st_size);
        }
    }

    if (closedir(dir))
        ERR("closedir");

    if (entry == NULL && errno)
        ERR("readdir");

    return;
}

int main(int argc, char** argv)
{
    if (argc <= 1)
        usage();
    
    if (strcmp(argv[1], "-p") != 0)
        usage();
    
    basicDirectionScan(argv[2]);
    return EXIT_SUCCESS;
}