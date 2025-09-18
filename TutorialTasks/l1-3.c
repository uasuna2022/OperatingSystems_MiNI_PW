#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXOPENFD 50
#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int files = 0, links = 0, directories = 0, others = 0;

int walk(const char* name, const struct stat* st, int flag, struct FTW* ftw)
{
    if (S_ISDIR(st->st_mode))
        directories++;
    else if (S_ISREG(st->st_mode))
        files++;
    else if (S_ISLNK(st->st_mode))
        links++;
    else others++;
    
    return 0;
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
        
        if (!nftw(argv[i], walk, MAXOPENFD, FTW_PHYS))
            printf("Files: %d\nLinks: %d\nDirectories: %d\nOthers: %d\n", files, links, directories, others);
        else printf("Access denied!");
        files = 0; links = 0; directories = 0; others = 0;

        if(chdir(start_path))
            ERR("chdir");
    }

    return EXIT_SUCCESS;
}