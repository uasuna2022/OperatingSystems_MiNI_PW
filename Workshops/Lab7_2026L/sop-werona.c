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

struct ClientInfo {
    int active;
    // Bufory na imiona (MAX_MSG_LEN + 1 na znak '\0')
    char name[MAX_MSG_LEN + 1];
    int name_len;        // Ile liter imienia już wczytano
    int name_complete;   // Czy znaleźliśmy już '\n' dla imienia?

    char partner[MAX_MSG_LEN + 1];
    int partner_len;
    int partner_complete; // Czy znaleźliśmy już '\n' dla partnera?
};

struct ClientInfo clients[1024];

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


    memset(clients, 0, sizeof(clients));

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
            // === SCENARIUSZ 1: NOWE POŁĄCZENIE ===
            if (events[i].data.fd == server_fd)
            {
                int client_fd = add_new_client(server_fd);
                if (client_fd >= 0)
                {
                    // Resetujemy stan tego konkretnego klienta w naszej tablicy
                    clients[client_fd].active = 1;
                    clients[client_fd].name_len = 0;
                    clients[client_fd].name_complete = 0;
                    clients[client_fd].partner_len = 0;
                    clients[client_fd].partner_complete = 0;
                    memset(clients[client_fd].name, 0, MAX_MSG_LEN + 1);
                    memset(clients[client_fd].partner, 0, MAX_MSG_LEN + 1);

                    // Rejestrujemy klienta w epoll (dodajemy EPOLLRDHUP żeby łapać rozłączenia)
                    ev.events = EPOLLIN | EPOLLRDHUP;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                        ERR("epoll_ctl ADD client");
                }
            }
            // === SCENARIUSZ 2: ZNANY KLIENT COŚ ZROBIŁ ===
            else 
            {
                int c_fd = events[i].data.fd;

                // Sprawdzamy, czy klient się przypadkiem nie rozłączył (EOF lub błąd)
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {
                    // Wymóg z etapu 2: Zależnie od tego, czy podał imię wypisujemy różne rzeczy
                    if (clients[c_fd].name_complete) {
                        printf("Utraciłem kontakt z %s\n", clients[c_fd].name);
                    } else {
                        printf("Utraciłem kontakt z ??\n");
                    }

                    // Sprzątanie po kliencie
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                    close(c_fd);
                    clients[c_fd].active = 0;
                    continue; // Ważne: przerywamy obieg pętli dla tego FD, bo już go zamknęliśmy
                }

                // Odczyt danych z gniazda
                char buffer[256];
                int bytes_read = read(c_fd, buffer, sizeof(buffer));

                if (bytes_read > 0)
                {
                    // Zamiast traktować cały bufor jako jedną wiadomość, 
                    // przetwarzamy go literka po literce, szukając '\n'
                    for (int j = 0; j < bytes_read; j++)
                    {
                        char c = buffer[j];

                        // Krok A: Czytamy imię klienta
                        if (!clients[c_fd].name_complete) 
                        {
                            if (c == '\n') {
                                clients[c_fd].name_complete = 1; // Mamy całe imię!
                            } else if (clients[c_fd].name_len < MAX_MSG_LEN) {
                                clients[c_fd].name[clients[c_fd].name_len++] = c;
                            }
                        }
                        // Krok B: Imię już mamy, więc czytamy imię partnera
                        else if (!clients[c_fd].partner_complete) 
                        {
                            if (c == '\n') {
                                clients[c_fd].partner_complete = 1; // Mamy partnera!
                            } else if (clients[c_fd].partner_len < MAX_MSG_LEN) {
                                clients[c_fd].partner[clients[c_fd].partner_len++] = c;
                            }
                        }
                    }

                    // Po przetworzeniu odebranych bajtów sprawdzamy, czy mamy już komplet informacji
                    if (clients[c_fd].name_complete && clients[c_fd].partner_complete)
                    {
                        // Wymóg etapu 2: wypisujemy kto z kim chce wziąć ślub
                        printf("%s chce pobrać się z %s\n", clients[c_fd].name, clients[c_fd].partner);
                        
                        // Zgodnie z poleceniem na tym etapie: od razu po tym rozłączamy klienta
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                        close(c_fd);
                        clients[c_fd].active = 0;
                    }
                }
                else if (bytes_read == 0) // Tradycyjny EOF (gdyby EPOLLRDHUP nie zadziałał)
                {
                    if (clients[c_fd].name_complete) {
                        printf("Utraciłem kontakt z %s\n", clients[c_fd].name);
                    } else {
                        printf("Utraciłem kontakt z ??\n");
                    }
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                    close(c_fd);
                    clients[c_fd].active = 0;
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
