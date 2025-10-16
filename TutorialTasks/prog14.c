#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define FILE_BUFFER_SIZE 256

void usage()
{
    char* msg = "USAGE: ./progfilename path1 path2\n";
    write(2, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

int readFromFile(int fd, char* buf, off_t currPosition)
{
    if (lseek(fd, currPosition, SEEK_SET) < 0)
        ERR("lseek");
    
    int bytesRead = read(fd, buf, FILE_BUFFER_SIZE);
    if (bytesRead < 0 && errno != EINTR)
        ERR("read");
    if (bytesRead < 0 && errno == EINTR)
        return 0; // if read was interrupted by signal, return 0 bytes read (and the main loop will try to read again)
    
    return bytesRead;
}

int writeToFile(int fd, char* buf, int bytesToWrite)
{
    int bytesWritten = write(fd, buf, bytesToWrite);
    if (bytesWritten < 0)
        ERR("write");
    
    return bytesWritten;
}

int main (int argc, char** argv)
{
    if (argc != 3) // expecting 2 arguments (otherwise program usage is incorrect)
        usage();

    char* path1 = argv[1]; 
    int fd = open(path1, O_RDONLY); // open file for reading only (otherwise error: i.e. file does not exist)
    if (fd < 0)
        ERR("open");
    
    struct stat st; // stat structure to hold the information about the file
    if (fstat(fd, &st) < 0) // get the info about the file
        ERR("fstat");
    if (!S_ISREG(st.st_mode)) // if the file is not a regular file (i.e. it's a directory or some sort of special file)
        ERR("not a file"); // exit with error

    char* path2 = argv[2];
    int fd2 = open(path2, O_WRONLY | O_CREAT | O_APPEND, 0644); // open file for writing only, create if it does not exist, append to the end of the file
    if (fd2 < 0)
        ERR("open2");
    
    char buffer[FILE_BUFFER_SIZE];
    off_t currPosition = 0; // initialize a variable to store the current position in the file
    int bytesRead;

    while ((bytesRead = readFromFile(fd, buffer, currPosition)) > 0) // main loop to read from file
    {
        int bytesWritten = writeToFile(fd2, buffer, bytesRead); // write to the second file
        if (bytesWritten != bytesRead) // if not all read bytes were written - exit with error 
            ERR("partial write"); 

        currPosition += bytesRead; // update the current position in the file
        sleep(1);
    }


    close(fd);
    close(fd2);
    
    return EXIT_SUCCESS;     
}