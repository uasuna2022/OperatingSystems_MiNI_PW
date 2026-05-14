# Notatka do Lab7 - Protokoły Strumieniowe i epoll

## Gniazda (sockets)
**Gniazdo** - to abstrakcyjny, wirtualny punkt końcowy, który umożliwia dwukierunkową komunikację między procesami. W systemie Linux jest ono reprezentowane jako *zwykły deskryptor pliku*. 
Zamiast pisać dane na dysk twardy, zapisujemy je do tego wirtualnego pliku, a OS dba o to, aby dane trafiły do odpowiedniego procesu odbierającego (przy czym może to być proces na tym samym komputerze lub na innym komputerze w sieci).

Gniazda rozwiązują główny problem komunikacji międzyprocesowej (IPC) - umożliwiają wymianę danych między procesami, które **mogą być uruchomione na różnych maszynach**. Dzięki gniazdom, procesy mogą komunikować się ze sobą bez względu na to, gdzie są uruchomione.

### Podział gniazd

#### Domeny (rodzaje gniazd)

Domeny gniazd określają, w jakiej przestrzeni adresowej będą działać gniazda. 

- **AF_UNIX (AF_LOCAL)** - gniazda działające w przestrzeni lokalnej, umożliwiają komunikację między procesami na tym samym komputerze. Adresy gniazd AF_UNIX są reprezentowane jako ścieżki do plików w systemie plików. 
Np. `/tmp/mój_gniazdo` - to jest adres gniazda AF_UNIX. Są one szybsze niż gniazda sieciowe, ponieważ komunikacja odbywa się bezpośrednio w pamięci, bez konieczności przechodzenia przez stos sieciowy.

- **AF_INET** - gniazda działające w przestrzeni adresowej IPv4, umożliwiają komunikację między procesami na różnych komputerach w sieci. Adresy gniazd AF_INET są reprezentowane jako kombinacja adresu IP i numeru portu. Np. `192.168.1.1:8080`. Gniazda AF_INET są bardziej uniwersalne, ponieważ mogą być używane do komunikacji zarówno lokalnej, jak i zdalnej, ale są wolniejsze niż gniazda AF_UNIX ze względu na konieczność przechodzenia przez stos sieciowy.

#### Typy gniazd

- **SOCK_STREAM** - gniazda strumieniowe, które zapewniają niezawodną, dwukierunkową komunikację. Są one oparte na protokole TCP (Transmission Control Protocol). Gniazda SOCK_STREAM gwarantują, że dane są dostarczane w tej samej kolejności, w jakiej zostały wysłane, oraz że nie zostaną utracone ani zduplikowane. Są idealne do aplikacji, które wymagają niezawodnej komunikacji, takich jak przeglądarki internetowe, serwery WWW czy aplikacje bazodanowe. 
Nie mają pojęcia granicy wiadomości, co pociąga za sobą konieczność implementacji własnego protokołu komunikacyjnego, który będzie rozdzielał dane na wiadomości.

- **SOCK_DGRAM** - gniazda datagramowe, które działają jak wysyłanie listów pocztą. Może ona nie dotrzeć do odbiorcy, może dotrzeć w innej kolejności niż została wysłana, a nawet może zostać zduplikowana. Gniazda SOCK_DGRAM są oparte na protokole UDP (User Datagram Protocol). Są one szybsze niż gniazda strumieniowe, ale nie gwarantują niezawodności ani kolejności dostarczania danych. Są idealne do aplikacji, które mogą tolerować utratę danych lub wymagają niskich opóźnień, takich jak gry online, transmisje wideo czy VoIP (Voice over IP).

### Podstawowe funkcje do pracy z gniazdami

- `socket(int domain, int type, int protocol)` - tworzy nowe gniazdo. `protocol` jest zwykle ustawiany na 0, co oznacza, że system operacyjny wybierze odpowiedni protokół na podstawie domeny i typu gniazda.
- `bind(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen)` - przypisuje adres do gniazda. Jest to konieczne dla serwerów, które muszą nasłuchiwać na określonym porcie. `*addr` jest wskaźnikiem do struktury `sockaddr_un`, która zawiera informacje o adresie (np. adres IP i port).
- `listen(int sockfd, int backlog)` - ustawia gniazdo w tryb nasłuchiwania, co jest konieczne dla serwerów, które chcą akceptować połączenia przychodzące. `backlog` określa maksymalną liczbę połączeń, które mogą być oczekujące w kolejce. Jeśli liczba połączeń przekroczy `backlog`, nowe połączenia mogą być odrzucane lub ignorowane, w zależności od implementacji systemu operacyjnego.
- `accept(int sockfd, struct sockaddr_un *addr, socklen_t *addrlen)` - blokuje program i czeka aż w kolejce pojawi się połączenie przychodzące, czyli nowy klient, który chce się połączyć z serwerem. Po zaakceptowaniu połączenia, `accept` zwraca nowy deskryptor pliku, który jest używany do komunikacji z tym konkretnym klientem. `*addr` jest wskaźnikiem do struktury `sockaddr_un`, która zostanie wypełniona informacjami o adresie klienta (np. jego adres IP i port).
- `connect(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen)` - jest używana przez klienta do nawiązania połączenia z serwerem. `*addr` jest wskaźnikiem do struktury `sockaddr_un`, która zawiera informacje o adresie serwera (np. jego adres IP i port).
- `read(int fd, void *buf, size_t count)` - odczytuje dane z gniazda. `fd` jest deskryptorem pliku gniazda, `*buf` jest wskaźnikiem do bufora, do którego zostaną zapisane odczytane dane, a `count` określa maksymalną liczbę bajtów do odczytania. 
- `write(int fd, const void *buf, size_t count)` - zapisuje dane do gniazda. `fd` jest deskryptorem pliku gniazda, `*buf` jest wskaźnikiem do bufora, który zawiera dane do wysłania, a `count` określa liczbę bajtów do wysłania. (W sumie funkcje `read` i `write` działają dokładnie tak samo, jak i w przypadku zwykłych plików.)
- `close(int fd)` - zamyka gniazdo, zwalniając zasoby systemowe związane z tym gniazdem. Po zamknięciu gniazda, jego deskryptor pliku staje się nieaktywny i nie można go już używać do komunikacji. Jest to ważne, aby uniknąć wycieków zasobów i zapewnić, że system operacyjny może efektywnie zarządzać gniazdami.

- `struct sockaddr_un` - struktura używana do reprezentowania adresu gniazda AF_UNIX. Zawiera pole `sun_family`, które określa rodzinę adresową (zawsze ustawiane na AF_UNIX), oraz pole `sun_path`, które zawiera ścieżkę do pliku reprezentującego gniazdo.
```c
struct sockaddr_un {
    sa_family_t sun_family; // tutaj zawsze wpisujemy AF_UNIX
    char sun_path[108];     // tutaj wpisujemy ścieżkę do pliku reprezentującego gniazdo, np. "/tmp/mój_gniazdo"
}

struct sockaddr_un addr;
memset(&addr, 0, sizeof(struct sockaddr_un));
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "tmp/my_socket", sizeof(addr.sun_path) - 1);

// i potem np.:
server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
bind(server_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un));
```

- `struct sockaddr_in` - struktura używana do reprezentowania adresu gniazda AF_INET. Zawiera pole `sin_family`, które określa rodzinę adresową (zawsze ustawiane na AF_INET), pole `sin_port`, które zawiera numer portu (w sieciowym porządku bajtów), oraz pole `sin_addr`, które zawiera adres IP (w sieciowym porządku bajtów).
```c
struct sockaddr_in {
    sa_family_t sin_family; // tutaj zawsze wpisujemy AF_INET
    in_port_t sin_port;     // tutaj wpisujemy numer portu, np. htons(8080)
    struct in_addr sin_addr; // tutaj wpisujemy adres IP, np. inet_pton (AF_INET, "192.168.1.1")
}
struct in_addr {
    uint32_t s_addr; // adres IP w sieciowym porządku bajtów (32-bitowa liczba całkowita)
}
struct sockaddr_in addr;
memset(&addr, 0, sizeof(struct sockaddr_in));
addr.sin_family = AF_INET;
addr.sin_port = htons(8080); // numer portu, konwertowany do sieciowego porządku bajtów
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr); // adres IP, konwertowany do sieciowego porządku bajtów
```

- `htons(uint16_t hostshort)` - konwertuje 16-bitową liczbę całkowitą z porządku bajtów hosta (host byte order) do porządku bajtów sieci (network byte order). Jest to ważne, ponieważ różne architektury komputerowe mogą używać różnych porządków bajtów, a sieć wymaga jednolitego formatu. `htons` jest używane głównie do konwersji **numerów portów**.
- `htonl(uint32_t hostlong)` - konwertuje 32-bitową liczbę całkowitą z porządku bajtów hosta do porządku bajtów sieci. Jest to używane głównie do konwersji **adresów IP**.
- `inet_pton(int af, const char *src, void *dst)` - konwertuje adres IP z formatu tekstowego na format binarny. `af` określa rodzinę adresową (np. AF_INET), `*src` jest wskaźnikiem do łańcucha znaków zawierającego adres IP w formacie tekstowym (np. "192.168.1.1"), a `*dst` jest wskaźnikiem do bufora, do którego zostanie zapisany adres IP w formacie binarnym (np. struktura `in_addr`).
```c
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
```

## Epoll (event polling)
Istnieje problem, że jeżeli wywołujemy funkcję `read`, to program się zawiesza aż do momentu, kiedy dane będą dostępne do odczytania. Jeśli mamy obsługiwać np. 100 klientów, to musielibyśmy mieć 100 wątków, z których każdy będzie wywoływał `read` i czekał na dane od swojego klienta. Jest to zbyt nieefektywne ze względu na zużycie zasobów systemowych (każdy wątek zajmuje pamięć i czas procesora). Moglibyśmy też potencjalnie w pętli sprawdzać każde z gniazd, ale to również jest nieefektywne, ponieważ większość czasu spędzalibyśmy na sprawdzaniu gniazd, które nie mają danych do odczytania.

**Epoll** jest mechanizmem do sterowania zdarzeniami. Zamiast pytać każdego klienta "Masz coś dla mnie?", mówimy OS, że tu mamy listę gniazd, które nas interesują, i że chcemy być powiadomieni, kiedy któreś z nich będzie miało dane do odczytania. OS będzie monitorował te gniazda i powiadomi nas tylko wtedy, gdy któreś z nich będzie gotowe do odczytania danych. Dzięki temu możemy obsługiwać wiele klientów za pomocą jednego wątku, co jest znacznie bardziej efektywne.

### Podstawowe funkcje do pracy z epoll

- `int epoll_fd = epoll_create1(int flags)` - tworzy nową instancję epoll i zwraca jej deskryptor pliku. `flags` zazwyczaj jest ustawiany na 0, ale można użyć `EPOLL_CLOEXEC`, aby automatycznie zamknąć epoll podczas wykonywania nowego programu (np. po wywołaniu `exec`). Ten deskryptor pliku jest używany do zarządzania zdarzeniami epoll.
Pod spodem OS tworzy tzw. listę gotowności (readiness list), która jest strukturą danych przechowującą informacje o gniazdach, które nas interesują, i ich aktualnym stanie (czy są gotowe do odczytania, zapisu itp.). Kiedy wywołujemy `epoll_wait`, OS sprawdza tę listę i zwraca nam tylko te gniazda, które są gotowe do obsługi.

- `int epoll_ctl(int epoll_fd, int operation, int fd, struct epoll_event* event)` - dodaje, modyfikuje lub usuwa gniazdo z instancji epoll. `epoll_fd` to deskryptor pliku epoll, `operation` określa operację (np. `EPOLL_CTL_ADD` do dodania gniazda, `EPOLL_CTL_MOD` do modyfikacji istniejącego gniazda, `EPOLL_CTL_DEL` do usunięcia gniazda), `fd` to deskryptor pliku gniazda, a `*event` jest wskaźnikiem do struktury `epoll_event`, która zawiera informacje o zdarzeniach, które nas interesują (np. `EPOLLIN` dla gotowości do odczytu).
```c
struct epoll_event {
    uint32_t events; // tutaj określamy, jakie zdarzenia nas interesują, np. EPOLLIN
    struct epoll_data {
        int fd;
        // ptr_t ptr;
        // uint32_t u32;
        // uint64_t u64;
    } data;
}

struct epoll_event event;
memset(&event, 0, sizeof(struct epoll_event));
event.events = EPOLLIN;
event.data.fd = client_fd; // np. deskryptor pliku gniazda klienta, które chcemy monitorować
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event); // dodajemy gniazdo klienta do epoll, aby być powiadomionym, kiedy będzie gotowe do odczytania danych
```
Możliwe flagi w `events`:
- `EPOLLIN` - gotowość do odczytu (dane są dostępne do odczytania)
- `EPOLLOUT` - gotowość do zapisu (można zapisać dane bez blokowania)
- `EPOLLERR` - wystąpił błąd na gnieździe
- `EPOLLHUP` - gniazdo zostało zamknięte
- `EPOLLRDHUP` - druga strona zamknęła połączenie (dotyczy gniazd strumieniowych)

Zazwyczaj dla klienta włączamy flagi `EPOLLIN | EPOLLRDHUP`, ponieważ interesuje nas, kiedy klient będzie miał dane do odczytania, oraz kiedy zamknie połączenie. Dla gniazda serwera, które nasłuchuje na nowe połączenia, włączamy flagę `EPOLLIN`, ponieważ interesuje nas, kiedy pojawi się nowe połączenie do zaakceptowania.

- `int epoll_wait(int epoll_fd, struct epoll_event* events, int maxevents, int timeout)` - czeka na zdarzenia na gniazdach monitorowanych przez epoll. `epoll_fd` - deskryptor pliku epoll, `*events` jest wskaźnikiem do bufora, do którego zostaną zapisane informacje o zdarzeniach, `maxevents` określa maksymalną liczbę zdarzeń, które mogą być zwrócone, a `timeout` określa czas oczekiwania w milisekundach (np. -1 oznacza czekanie bez ograniczeń czasowych). Funkcja zwraca liczbę zdarzeń, które zostały zwrócone, lub -1 w przypadku błędu.
```c
struct epoll_event events[10]; // bufor na zdarzenia, który może pomieścić informacje o maksymalnie 10 zdarzeniach
int num_events = epoll_wait(epoll_fd, events, 10, -1); // czekamy na zdarzenia bez ograniczeń czasowych
if (num_events == -1) {
    perror("epoll_wait");
    exit(EXIT_FAILURE);
}   
for (int i = 0; i < num_events; i++) {
    if (events[i].events & EPOLLIN) {
        // tutaj obsługujemy zdarzenie gotowości do odczytu dla gniazda o deskryptorze events[i].fd
    }
    if (events[i].events & EPOLLRDHUP) {
        // tutaj obsługujemy zdarzenie zamknięcia połączenia przez klienta dla gniazda o deskryptorze events[i].fd
    }
}
```

### Inne ważne "haczyki" dotyczące epoll

- Jeśli klient się rozłączy, to próba odczytania danych z jego gniazda zwróci 0 bajtów, co oznacza, że druga strona zamknęła połączenie. W takim przypadku powinniśmy usunąć to gniazdo z epoll (za pomocą `epoll_ctl` z `EPOLL_CTL_DEL`) i zamknąć je (za pomocą `close`), aby zwolnić zasoby systemowe.
- Jeśli klient się rozłączy, a my mamy włączoną flagę `EPOLLRDHUP`, to epoll automatycznie powiadomi nas o tym zdarzeniu, co pozwala nam szybciej zareagować na rozłączenie klienta.
- Jeśli klient się rozłączy, a my NIE mamy włączonej flagi `EPOLLRDHUP` i serwer spróbuje zapisać dane do tego klienta, to funkcja `write` zwróci błąd `EPIPE`, co oznacza, że druga strona zamknęła połączenie. `EPIPE` domyślnie powoduje wysłanie `SIGPIPE` co może zakończyć nasz program serwera, dlatego warto obsłużyć (albo zignorować) ten sygnał, aby nasz serwer nie zakończył się nieoczekiwanie.
- Jeśli program serwera zostanie przerwany (np. przez naciśnięcie Ctrl+C), to powinien on zamknąć wszystkie otwarte gniazda i usunąć plik gniazda (w przypadku AF_UNIX) za pomocą `unlink`, aby zwolnić zasoby systemowe i uniknąć problemów przy ponownym uruchomieniu serwera. Robimy to w bloku obsługi sygnału `SIGINT`, który jest wysyłany do programu, gdy użytkownik naciśnie Ctrl+C. W tym bloku zamykamy wszystkie otwarte gniazda i usuwamy plik gniazda, a następnie kończymy program. Inaczej procesy klienckie mogą zostać "zawieszone" w oczekiwaniu na odpowiedź od serwera, który już nie istnieje, co może prowadzić do wycieków zasobów i innych problemów. Jednak zazwyczaj OS robi tą pracę za nas.


### Klasyczny workflow dla serwera korzystającego z epoll

Poniższy dokument opisuje krok po kroku budowę asynchronicznego serwera w języku C z wykorzystaniem mechanizmu `epoll` i gniazd z domeny UNIX (`AF_UNIX`).

#### FAZA 0: Struktury Danych i Przygotowanie

W aplikacjach takich jak zadanie z Weroną, serwer musi "pamiętać", kim są jego klienci. Zanim zaczniemy pisać logikę sieciową, musimy stworzyć strukturę pamięci dla klientów.

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 256
#define SOCKET_PATH "/tmp/werona_socket"
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// STRUKTURA PAMIĘCI SERWERA
// Używamy numeru deskryptora (fd) jako indeksu w tablicy, aby mieć szybki dostęp do danych.
struct ClientInfo {
    int is_active;     // 1 jeśli ktoś tu jest, 0 jeśli puste
    char name[64];     // np. "Romeo\n"
    // Tutaj możesz dodać inne zmienne potrzebne w zadaniu (np. bufor do wczytywania po literce)
};

// Globalna tablica pamięci dla klientów (rozmiar wystarczający dla większości systemów)
struct ClientInfo clients[1024]; 

int main() {
    // 1. Ochrona przed crashem: Jeśli serwer spróbuje pisać do zamkniętego gniazda,
    // system wyśle SIGPIPE. Ignorujemy go, aby serwer działał dalej.
    signal(SIGPIPE, SIG_IGN);
```

#### FAZA 1: "Recepcja" - Tworzenie gniazda serwera.
Serwer musi utworzyć fizyczny punkt styku (plik), do którego będą podłączać się klienci (Netcat).

```c
int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) ERR("socket");

    // Zabezpieczenie: Kasujemy stary plik gniazda, gdyby serwer poprzednio padł
    unlink(SOCKET_PATH); 

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bindowanie (przypisanie adresu do gniazda)
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) ERR("bind");

    // Rozpoczęcie nasłuchiwania. '5' to długość kolejki oczekujących (backlog).
    if (listen(server_fd, 5) == -1) ERR("listen");
```

#### FAZA 2: "Strażnik" - Inicjalizacja epoll 
```c
// Tworzymy instancję epoll. Flaga 0 to domyślne, nowoczesne zachowanie.
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) ERR("epoll_create1");

    // Rejestrujemy gniazdo serwera (nasłuchujące) w epollu.
    struct epoll_event event;
    event.events = EPOLLIN;          // Chcemy wiedzieć o nowych połączeniach (odczyt)
    event.data.fd = server_fd;       // ID tego zdarzenia to numer naszego gniazda serwera

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) ERR("epoll_ctl: server");

    // Pusta tablica, do której system będzie wpisywał powiadomienia po wybudzeniu
    struct epoll_event events[MAX_EVENTS]; 
    
    printf("Serwer uruchomiony...\n");
```

#### FAZA 3: Serce programu - Pętla główna i rozdzielacz
```c
while (1) {
        // PROGRAM ZASYPIA TUTAJ.
        // Czeka na zdarzenie max 10 sekund (10000 milisekund).
        int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, 10000);

        // Obsługa przerwań systemowych (np. przez sygnały w tle)
        if (ready_fds == -1) {
            if (errno == EINTR) continue; 
            ERR("epoll_wait");
        }

        // Timeout - serwer wyłącza się, jeśli od 10s nic się nie dzieje
        if (ready_fds == 0) {
            printf("Brak aktywności. Zamykam serwer.\n");
            break; 
        }

        // System wybudził program! Analizujemy listę powiadomień.
        for (int i = 0; i < ready_fds; i++) {
            
            // =================================================================
            // GAŁĄŹ A: NOWY KLIENT (Zdarzenie dotyczy gniazda serwera)
            // =================================================================
            if (events[i].data.fd == server_fd) {
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd == -1) { perror("accept"); continue; }

                // 1. Zapisujemy klienta do naszej aplikacji (STAN)
                clients[client_fd].is_active = 1;
                memset(clients[client_fd].name, 0, sizeof(clients[client_fd].name));

                // 2. Oddajemy klienta pod obserwację epoll (SYSTEM)
                event.events = EPOLLIN | EPOLLRDHUP; // Interesują nas dane oraz rozłączenie
                event.data.fd = client_fd;           // Zapisujemy ID klienta, żeby system nam je zwrócił
                
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl: add client");
                    close(client_fd);
                }
                printf("[+] Nowy klient! (FD: %d)\n", client_fd);
            }

            // =================================================================
            // GAŁĄŹ B: ZNANY KLIENT COŚ ROBI
            // =================================================================
            else {
                int c_fd = events[i].data.fd; // Wyciągamy ID klienta z powiadomienia

                // SCENARIUSZ B1: Klient się rozłączył
                // UWAŻAJ NA NAWIASY: (EPOLLRDHUP | EPOLLHUP | EPOLLERR)
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    printf("[-] Klient się rozłączył (FD: %d)\n", c_fd);
                    
                    // Usuwamy klienta z systemu (epoll) i pamięci (tablica)
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                    close(c_fd);
                    clients[c_fd].is_active = 0;
                    continue; // Przechodzimy do kolejnego zdarzenia w pętli for
                }

                // SCENARIUSZ B2: Klient przesłał dane
                char buffer[BUFFER_SIZE];
                ssize_t bytes = read(c_fd, buffer, sizeof(buffer) - 1);

                if (bytes > 0) {
                    buffer[bytes] = '\0'; // Zabezpieczenie końca stringa
                    printf("Otrzymano od %d: %s", c_fd, buffer);

                    // MIEJSCE NA LOGIKĘ BIZNESOWĄ (Zadanie z Weroną)
                    // Wiemy z jakiego FD przyszły dane, więc możemy sprawdzić jego stan:
                    // if (clients[c_fd].name[0] == '\0') { 
                    //     // To jego pierwsza wiadomość - to musi być imię!
                    //     strncpy(clients[c_fd].name, buffer, bytes);
                    // } else {
                    //     // Ma już imię, to pewnie szuka partnera!
                    //     // Logika łączenia w pary...
                    // }

                    // Przykładowe odesłanie echa
                    write(c_fd, "Otrzymano!\n", 11);
                } 
                // Tradycyjny sposób wykrycia końca pliku (EOF)
                else if (bytes == 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c_fd, NULL);
                    close(c_fd);
                    clients[c_fd].is_active = 0;
                }
            }
        } // Koniec pętli for (przetworzono wszystkie zdarzenia)
    } // Koniec pętli while (program wraca do snu w epoll_wait)
```

#### FAZA 4: Sprzątanie po sobie
```c
    close(server_fd);
    close(epoll_fd);
    unlink(SOCKET_PATH);
    return 0;
}
```



