#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage()
{
    char* msg = "USAGE: -p dir1 -p dir2 ... -p dirn -o file\n";
    write(2, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void scanDirectory(char* dirPath, int fd)
{
    char originalPath[200];
    if (!getcwd(originalPath, 200))
        ERR("getcwd");
    
    DIR* currentDirectory = opendir(dirPath);
    if (!currentDirectory && errno == ENOENT)
        ERR("directory doesn't exist");
    if (!currentDirectory && errno != ENOENT)
        ERR("opendir");
    if (chdir(dirPath))
        ERR("chdir");

    char curDirFullPath[200];
    if (!getcwd(curDirFullPath, 200))
        ERR("getcwd");

    dprintf(fd, "Directory full path: %s\n", curDirFullPath);
    dprintf(fd, "File list:\n");

    struct dirent* entry;
    struct stat st;

    while(1)
    {
        entry = readdir(currentDirectory);
        if (!entry && errno == 0)
            break;
        if (!entry && errno != 0)
            ERR("readdir");
        
        if (lstat(entry->d_name, &st) < 0)
            ERR("lstat");
        
        if (S_ISREG(st.st_mode))
            dprintf(fd, "\t%s\t REG\t%ld\n", entry->d_name, st.st_size);
        
        if (S_ISLNK(st.st_mode))
            continue;
        
        if (S_ISDIR(st.st_mode))
            dprintf(fd, "\t%s\t DIR\t\n", entry->d_name);
    }

    if (closedir(currentDirectory))
        ERR("closedir");

    if (chdir(originalPath))
        ERR("chdir");
}

int main(int argc, char** argv)
{
    if (argc <= 1)
        usage();

    if (strcmp(argv[argc - 2], "-o") != 0)
        usage();
    
    int fd = open(argv[argc - 1], O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0644);
    if (fd < 0)
        ERR("open");
    
    char* directoryPaths[100];
    int i = 1;
    int numberOfDirectories = 0;
    while(1)
    {
        if (i >= argc - 2)
            break;
        if (strcmp(argv[i], "-p") == 0)
        {
            directoryPaths[numberOfDirectories] = argv[i + 1];
            numberOfDirectories++;
            i += 2;
            continue;
        }
        else i++;
    }

    for (int j = 0; j < numberOfDirectories; j++)
    {
        scanDirectory(directoryPaths[j], fd);
    }
}