#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define SOCKET_PATH "/tmp/echo_socket"
#define MAX_EVENTS 10
#define BUFFER_SIZE 256


int main (int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);
    int server_fd, client_fd, epoll_fd;
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
        ERR("listen");
    
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        ERR("epoll_create1");
    
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
        ERR("epoll_ctl: server_fd");
    
    printf("Server is listening on %s\n", SOCKET_PATH);
    while (1)
    {
        int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, 100000);
        if (ready_fds == -1)
            ERR("epoll_wait");
        if (ready_fds == 0)
        {
            printf("No events in 100 seconds, exiting...\n");
            break;
        }

        for (int i = 0; i < ready_fds; i++)
        {
            // Nowy klient próbuje się połączyć
            if (events[i].data.fd == server_fd)
            {
                client_fd = accept(server_fd, NULL, NULL);
                if (client_fd == -1)
                    ERR("accept");
                
                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, 
                    &event) == -1)
                    ERR("epoll_ctl: client_fd");
                printf("[+] New client connected: fd %d\n", client_fd);
            }

            else // Dane od klienta lub rozłączenie
            {
                client_fd = events[i].data.fd;
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) // klient się rozłączył
                {
                    printf("[-] Client disconnected: fd %d\n", client_fd);
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
                        ERR("epoll_ctl: del client_fd");
                    close(client_fd);
                    continue;
                }

                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(client_fd, buffer,
                     sizeof(buffer) - 1);
                if (bytes_read > 0)
                {
                    buffer[bytes_read] = '\0';
                    printf("Received from client (fd %d): %s\n", client_fd, buffer);

                    // Convert to uppercase
                    for (ssize_t j = 0; j < bytes_read; j++)
                        buffer[j] = toupper((unsigned char)buffer[j]);

                    ssize_t bytes_written = write(client_fd, buffer, bytes_read);
                    if (bytes_written == -1)
                    {
                        printf("[-] Error writing to client fd %d, closing connection\n", client_fd);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
                            ERR("epoll_ctl: del client_fd");
                        close(client_fd);
                    }
                    else
                        printf("Sent to client (fd %d): %s\n", client_fd, buffer);
                }
                else if (bytes_read == 0) // klient się rozłączył
                {
                    printf("[-] Client disconnected: fd %d\n", client_fd);
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
                        ERR("epoll_ctl: del client_fd");
                    close(client_fd);
                }
                else // błąd odczytu
                {
                    printf("[-] Error reading from client fd %d, closing connection\n", client_fd);
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
                        ERR("epoll_ctl: del client_fd");
                    close(client_fd);
                }
            }
        }
    }

    printf("Shutting down server...\n");
    close(server_fd);
    close(epoll_fd);
    unlink(SOCKET_PATH);

    return EXIT_SUCCESS;
}