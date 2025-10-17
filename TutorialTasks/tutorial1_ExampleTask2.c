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

void usage() // prints program usage information and exits with failure code
{
    char* msg = "USAGE: ./progfilename -p dir1 -p dir2 -d 2 -p dir3 -d 3 -e txt -o ...\n";
    write(2, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void basicDirectoryScan(char* path, int out_fd)
{
    char originalPath[200];
    if (!getcwd(originalPath, sizeof(originalPath)))
        ERR("getcwd");

    DIR* dir = opendir(path);
    if (!dir && errno == ENOENT)
        ERR("directory does not exist");
    if (!dir && errno != ENOENT)
        ERR("opendir");

    if (chdir(path))
        ERR("chdir");
    
    char curDirFullPath[200];
    if (!getcwd(curDirFullPath, sizeof(curDirFullPath)))
        ERR("getcwd");
    
    struct dirent* entry;
    struct stat st;
    dprintf(out_fd, "path: %s\n", curDirFullPath);

    errno = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (lstat(entry->d_name, &st) < 0)
            ERR("lstat");
        if (S_ISREG(st.st_mode))
        {
            dprintf(out_fd, "\t%s\t", entry->d_name);
            dprintf(out_fd, "%ld\n", st.st_size);
        } 
    }

    if (closedir(dir))
        ERR("closedir");

    if (entry == NULL && errno)
        ERR("readdir");

    if (chdir(originalPath))
        ERR("chdir");

    return;
}

char* getFileExtension(char* filename) // returns a pointer to the file extension without the dot
{
    for (int i = strlen(filename) - 1; i >= 0; i--)
    {
        if (filename[i] == '.' && i != 0) // make sure the dot is not the first character
            return filename + i + 1;
    }

    return NULL; // no extension found
}

// checks if the file extension matches any of the passing extensions
int check(char* filename, char* extensions[], int numberOfPassingExtensions) 
{
    char* extension = getFileExtension(filename);
    if (!extension)
        return -1;
    
    for (int i = 0; i < numberOfPassingExtensions; i++)
    {
        if (strcmp(extension, extensions[i]) == 0)
            return 0;
    }

    return 1;
}

void directoryScanWithExtensions(char* path, char* extensions[], int numberOfPassingExtensions, int out_fd)
{
    char originalPath[200];
    if (!getcwd(originalPath, sizeof(originalPath)))
        ERR("getcwd");

    DIR* dir = opendir(path);
    if (!dir && errno == ENOENT)
        ERR("directory does not exist");
    if (!dir && errno != ENOENT)
        ERR("opendir");

    if (chdir(path))
        ERR("chdir");
    
    char curDirFullPath[200];
    if (!getcwd(curDirFullPath, sizeof(curDirFullPath)))
        ERR("getcwd");

    struct dirent* entry;
    struct stat st;
    dprintf(out_fd, "path: %s\n", curDirFullPath);

    errno = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (lstat(entry->d_name, &st) < 0)
            ERR("lstat");
        if (!S_ISREG(st.st_mode))
            continue;

        if (check(entry->d_name, extensions, numberOfPassingExtensions) == 0)
        {
            dprintf(out_fd, "\t%s\t", entry->d_name);
            dprintf(out_fd, "%ld\n", st.st_size);
        }
    }

    if (closedir(dir))
        ERR("closedir");

    if (entry == NULL && errno)
        ERR("readdir");
    
    if (chdir(originalPath))
        ERR("chdir");
        
    return;
}

int main(int argc, char** argv)
{
    if (argc <= 1)
        usage();
    
    if (strcmp(argv[1], "-p") != 0)
        usage();

    char* curDir;
    char* extensions[20];
    int numberOfPassingExtensions = 0;
    int readyForNewDirectory;

    int fd = 1;
    char* env = getenv("L1_OUTPUTFILE");
    //dprintf(2, "Output file from env: %s\n", env);

    if (strcmp(argv[argc - 1], "-o") == 0)
    {
        if (!env)
        {
            fprintf(stderr, "Environment variable L1_OUTPUTFILE not set\n");
            exit(EXIT_FAILURE);
        }
        fd = open(env, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0644);
        if (fd < 0)
            ERR("open");
    }

    int i = 1;
    readyForNewDirectory = 1;
    while(1)
    {
        if (i >= argc)
            break;
        
        if (strcmp(argv[i], "-p") == 0 && readyForNewDirectory)
        {
            if (i + 1 >= argc)
                usage();
            curDir = argv[i + 1];
            readyForNewDirectory = 0;
            i += 2;
            if (i == argc)
            {
                basicDirectoryScan(curDir, fd);
                readyForNewDirectory = 1;
                break;
            }
            continue;
        }

        if (strcmp(argv[i], "-p") == 0 && !readyForNewDirectory)
        {
            basicDirectoryScan(curDir, fd);
            readyForNewDirectory = 1;
            continue;
        }

        if (strcmp(argv[i], "-e") == 0)
        {
            if (i + 1 >= argc)
                usage();
            i++;
            int n = 0;
            while (i < argc && argv[i][0] != '-')
            {
                extensions[n] = argv[i];
                n++;
                i++;
            }
            numberOfPassingExtensions = n;

            if (i == argc || strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-o") == 0)
            {
                directoryScanWithExtensions(curDir, extensions, numberOfPassingExtensions, fd);
                readyForNewDirectory = 1;
                numberOfPassingExtensions = 0;
                continue;
            }
        }

        if (strcmp(argv[i], "-d") == 0)
        {
            // currently no depth handling implemented
            i += 2;
            continue;
        }   

        if (strcmp(argv[argc - 1], "-o") == 0 && i == argc - 1)
            break;
    }

    close(fd);

    return EXIT_SUCCESS;
}