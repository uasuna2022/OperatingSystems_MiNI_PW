#define _DEFAULT_SOURCE

#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/io.h>
#include <stdlib.h>

int main()
{
    int files = 0, links = 0, directories = 0, others = 0;
    DIR* curdir = opendir(".");
    if (!curdir)
    {
        perror("opendir");
        return EXIT_FAILURE; 
    }

    struct dirent* entry;

    while (1)
    {
        entry = readdir(curdir);
        if (!entry) break;

        if (entry->d_type == DT_DIR) directories++;
        else if (entry->d_type == DT_REG) files++;
        else if (entry->d_type == DT_LNK) links++;
        else others++;
    }

    if (errno) 
    {
        perror("readdir");
        return EXIT_FAILURE;
    }

    printf("Files: %d\nLinks: %d\nDirectories: %d\nOthers: %d\n", files, links, directories, others);
    if (closedir(curdir))
    {
        perror("closedir");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
