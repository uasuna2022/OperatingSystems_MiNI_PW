#include "l7-common.h"

void usage(char *name)
{
    printf("%s <timeout>\n", name);
    printf("  timeout - max waiting time after receiving the last message/connection (in seconds)\n");
    exit(EXIT_FAILURE);
}

#define SWAP(a, b)                      \
    do                                  \
    {                                   \
        __typeof__(a) __a = (a);        \
        __typeof__(b) __b = (b);        \
        __typeof__(*__a) __tmp = *__a;  \
        *__a = *__b;                    \
        *__b = __tmp;                   \
    } while (0)

#define MAX_CLIENTS 10
#define MAX_PAIRS 3
#define UNIX_SK_NAME "Laurenty"
#define MAX_MSG_LEN 63

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int timeout = atoi(argv[1]);
    if (timeout < 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    sethandler(SIG_IGN, SIGPIPE);

    // 1. Tworzymy gniazdo UNIX. 
    // Ta funkcja sama usuwa stary plik (unlink), robi socket, bind i listen.
    int server_fd = bind_local_socket(UNIX_SK_NAME, MAX_CLIENTS);

    // 2. Tworzymy naszego strażnika - epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) ERR("epoll_create1");

    // Rejestrujemy gniazdo serwera do nasłuchiwania w epoll
    struct epoll_event ev;
    ev.events = EPOLLIN; // Interesują nas nowe połączenia (odczyt na serwerze)
    ev.data.fd = server_fd; // Notatka dla nas, żebyśmy wiedzieli że to powiadomienie z serwera
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) 
        ERR("epoll_ctl ADD server");

    struct epoll_event events[MAX_CLIENTS];

    // 3. Główna pętla
    while (1)
    {
        // epoll_wait usypia program.
        int ready_fds = epoll_wait(epoll_fd, events, MAX_CLIENTS, timeout * 1000);

        if (ready_fds == -1)
        {
            if (errno == EINTR)  // Jak system przerwał, to po prostu próbujemy znowu
                continue; 
            ERR("epoll_wait");
        }

        // nfds == 0 oznacza, że czas minął (timeout) i nikt nie przyszedł
        if (ready_fds == 0)
        {
            printf("Nikt już nie potrzebuje mojej pomocy!\n");
            break; 
        }

        // Przeglądamy wszystkie powiadomienia, które przyniósł epoll
        for (int i = 0; i < ready_fds; i++)
        {
            // Jeśli powiadomienie dotyczy naszego gniazda "recepcji"...
            if (events[i].data.fd == server_fd)
            {
                // ...to znaczy, że ktoś nowy dzwoni. Odbieramy (znowu funkcja z l7-common)
                int client_fd = add_new_client(server_fd);
                if (client_fd >= 0)
                {
                    // Wymóg z etapu 1: Wypisać komunikat z numerem deskryptora
                    printf("Kolejna młoda osoba (%d) potrzebuje mojej pomocy!\n", client_fd);
                    
                    // Wymóg z etapu 1: Od razu zamknąć połączenie!
                    // Na razie nie dodajemy go do epolla, bo w 1 etapie nie gadamy z klientem.
                    close(client_fd);
                }
            }
        }
    }

    // 4. Sprzątanie
    close(epoll_fd);
    close(server_fd);
    
    // Zgodnie z poleceniem: "Nie pozostawiaj śladów". Usuwamy plik z dysku.
    // Jeśli pliku nie ma (ENOENT), to też dobrze, więc ignorujemy ten błąd.
    if (unlink(UNIX_SK_NAME) == -1 && errno != ENOENT) ERR("unlink");

    return EXIT_SUCCESS;
}
