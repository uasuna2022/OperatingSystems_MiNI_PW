#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void scan_directory()
{
    int files = 0, links = 0, directories = 0, others = 0;
    DIR* curdir = opendir(".");
    if (!curdir)
        perror("opendir");

    struct dirent* entry;

    while (1)
    {
        errno = 0;
        entry = readdir(curdir);
        if (!entry) break;

        if (entry->d_type == DT_DIR) directories++;
        else if (entry->d_type == DT_REG) files++;
        else if (entry->d_type == DT_LNK) links++;
        else others++;
    }

    if (errno && !entry) 
        perror("readdir");

    printf("Files: %d\nLinks: %d\nDirectories: %d\nOthers: %d\n", files, links, directories, others);

    if (closedir(curdir))
        perror("closedir");
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printf("You haven't entered any directory!\n");
        return EXIT_FAILURE;
    }

    char start_path[200];
    if (!getcwd(start_path, 200))
        ERR("getcwd");

    char buffer[200];
    for (int i = 1; i < argc; i++)
    {
        if (chdir(argv[i]))
            ERR("chdir");
        
        if (!getcwd(buffer, 200))
            ERR("getcwd");
        
        printf("Directory %s:\n", buffer);
        scan_directory();

        if(chdir(start_path))
            ERR("chdir");
    }

    return EXIT_SUCCESS;
}