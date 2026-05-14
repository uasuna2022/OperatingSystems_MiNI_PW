#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define SOCKET_PATH "/tmp/echo_socket"


// Echo server capital letters
int main(int argc, char** argv)
{
    if (argc > 1)
        return fprintf(stderr, "Usage: %s\n", argv[0]), EXIT_FAILURE;
    
    int server_fd, client_fd;
    struct sockaddr_un addr;
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
        ERR("socket");
    
    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, &addr, sizeof(struct sockaddr_un)) == -1)
        ERR("bind");
    
    if (listen(server_fd, 5) == -1)
    {
        close(server_fd);
        ERR("listen");
    }

    printf("Server is listening on %s\n", SOCKET_PATH);
    printf("Waiting for a client to connect...\n");

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1)
    {
        close(server_fd);
        ERR("accept");  
    }
    printf("Client connected.\n");

    char buffer[256];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1)
    {
        close(client_fd);
        close(server_fd);
        ERR("read");
    }

    if (bytes_read == 0)
    {
        printf("Client disconnected.\n");
        close(client_fd);
        close(server_fd);
        return EXIT_SUCCESS;
    }

    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        printf("Received from client: %s\n", buffer);

        // Convert to uppercase
        for (ssize_t i = 0; i < bytes_read; i++)
            buffer[i] = toupper((unsigned char)buffer[i]);

        ssize_t bytes_written = write(client_fd, buffer, bytes_read);
        if (bytes_written == -1)
        {
            close(client_fd);
            close(server_fd);
            ERR("write");
        }
        printf("Sent to client: %s\n", buffer);
    }

    printf("Closing connection.\n");
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return EXIT_SUCCESS;
}