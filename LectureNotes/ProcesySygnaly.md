# Sygnały - notatka

## Możliwe sygnały i ich zachowania
- `SIGTERM` (*Signal Termination*, *15*) - sygnał wysyłany zazwyczaj przez system UNIX, który grzecznie kończy proces w sposób kontrolowany i bezpieczny:
    + jest **przechwytywalny** (catchable) - można do niego zdefiniować własny handler
    + domyślna akcja: **Terminate**
    + proces domyślnie nie zapisuje danych i nie zwalnia zasobów
- `SIGINT` (*Signal Interruption*, *2*) - sygnał wysyłany zazwyczaj przez użytkownika w terminalu, który służy do zakończenia procesu (podobnie do `SIGTERM`)
    + jest **przechwytywalny** (catchable) - można do niego zdefiniować własny handler
    + domyślna akcja: **Terminate**
    + domyślnie wywoływany poprzez `Ctrl + C` (wysyła się domyślnie do procesu znajdującego się na pierwszym planie terminalu(foreground, fg))
    + też nie czyści po siebie nic i nie zwalnia zasobów
- `SIGKILL` (*Signal Kill*, *9*) - sygnał służący do brutalnego ostatecznego zakończenia procesu bez możliwości przechwytu tego
    + **NIE** jest **przechwytywalny** - nie można zdefiniować własnego handlera, a więc nie ma szansy na jakiekolwiek obsłużenie ręczne, zapis informacji i zwolnienie zasobów
    + może pozostawić dane w stanie spójnym, pliki w stanie otwartym itd.
    + używa się, kiedy program się zawiesił i nie ma innej opcji jego skończyć (np. nie reaguje na grzeczne sygnały terminujące)
- `SIGUSR1`, `SIGUSR2` (*User-Defined Sygnals*, *10*, *12*) - sygnały "puste", których system operacyjny nigdy nie wysyła samodzielnie.
    + są **przechwytywalne** (catchable)
    + domyślna akcja: **Terminate**
    + główna rola - komunikacja międzyprocesowa wewnątrz aplikacji napisanej przez programistę 
    + bez własnej definicji (handlera) są dokładnie takie same jak SIGINT/SIGTERM
- `SIGSTOP` (*Signal Stop*, *19*) -  sygnał używany do natychmiastowego zawieszenia procesu (pause).
    + **NIE** jest **przechwytywalny** - nie można go zignorować, zablokować ani przechwycić. 
    + domyślna akcja: **Zatrzymanie** 
    + w efekcie tego proces jest zamrażany w miejscu, przestaje używać CPU, ale pozostaje w pamięci i jego stan jest zapamiętywany
    + czeka na sygnał `SIGCONT`, aby wznowić swoje działanie
- `SIGCONT` (*Signal Continue*, *18*) - sygnał służący do wznowienia utrzymanego procesu.
    + jest **przechwytywalny**, chociaż w praktyce żadko to się robi, używa się raczej domyślnego handlera
    + domyślna akcja: **Wznowienie**
    + w efekcie tego proces, który był w stanie `Stopped` wznawia wykonywanie instrukcji od tego miejsca, w którym był zatrzymany
    + sygnał ten jest automatycznie wysyłany poprzez polecenia bashowe `bg <numer zadnia>` albo `fg <numer zadania>`
    + kiedy proces znajduje się w stanie `Stopped`, reaguje natychmiast tylko na 2 sygnały nieprzechwytywalne (`SIGKILL` i `SIGSTOP`, chociaż drugi nic nie robi) i na sygnał `SIGCONT`. Pozostałe sygnały są oczekujące. Trafiają do kolejki sygnałów i zostaje przetworzone dopiero w momencie, kiedy proces wznawia się.
- itd.

## Przydatne struktury i funkcje do obsługi sygnałów
- `struct sigaction`. Zdefiniowana w nagłówku <signal.h>. Służy do tego, żeby ustawić sposób reagowania na konkretne sygnały.
Pozwala ustawić czy sygnał ma być obsługiwany przez funkcję czy defaultowo, czy ma być ignorowany, poza tym pozwala ustawić maskę sygnału.
```c
// Przykład użycia tej struktury
#include <signal.h>

struct sigaction act;
memset(&act, 0, sizeof(struct sigaction)); // 0 - uzupełniamy wszystkie zarezerwowane bajty zerami

act.sa_handler = f; // f - jakaś zdefiniowana funkcja void 
act.sa_flags = SA_RESTART; // liczba 'int' - bitowa maska flag sterujących zachowaniem handlera
                           // np. SA_RESTART - jądro spróbuje automatycznie wznowić przerwane przez sygnał wywołanie systemowe
                           // SA_SIGINFO - system użyje pola sa_sigaction zamiast sa_handler
                           // SA_NOCLDWAIT - tylko dla SIGCHLD, zapobiega tworzeniu procesów zombie
sigemptyset(&sa.sa_mask);
sigaddset(&sa.sa_mask, SIGTERM);
sigaddset(&sa.sa_mask, SIGINT);
// zbiór sygnałów, które mają zostać dodatkowo zablokowane w trakcie wykonywania handlera
// czyli jak mamy już pewną maskę, to to pole dodaje DODATKOWE sygnałe do maski i po zakońćzeniu handlera wraca do stanu początkowego maski
// np. tutaj zostaną zablokowane dodatkowo sygnały SIGUSR1 i SIGINT na czas działania handlera

if (sigaction(SIGUSR1, &sa, NULL) == -1)
    ERR("sigaction");
```

- `sigaction(int sigNo, struct sigaction* act, struct sigaction* oact)` - funkcja, która ustawia nowe parametry obsługi sygnału 
(handler, maskę, flagi) dla podanego sygnału `sigNo`. Informację bierze z pola `act`, stary stan
kopiuje do obiektu, którego adres podany w polu `oact`. Zwraca 0 po sukcesie, -1 po błędzie.
- `sigset_t mask` - zmienna służącą do przechowywania zbioru sygnałów (zablokowanych, jakichkolwiek)
- `sigemptyset(sigset_t* mask)` - zapisuje do mask pusty zbiór sygnałów
- `sigfillset(sigset_t* mask)` - zapisuje do mask pełny zbiór sygnałów (wszystkich zdefiniowanych w POSIX, 
ignorując oczywiście `SIGKILL` i `SIGSTOP`, bo są to sygnały nieprzechwytywalne).
- `sigaddset(sigset_t* mask, int sigNo)`, `sigdelset(sigset_t* mask, int sigNo)` - dodaje lub usuwa podany sygnał 
`sigNo` do maski `mask`
- `sigismember(const sigset_t* mask, int sigNo)` - zwraca 1, jeśli sygnał `sigNo` znajduje się w zbiorze `mask`,
0 - jeśli nie znajduje się i -1 w przypadku błędu.
- `sethandler(void (*f)(int), int sigNo)` zwykle definiuje się w taki sposób:
```c
void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    
    // możliwa inicjalizacja dodatkowych pól struktury

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}
```
- `sigprocmask(int how, const sigset_t* mask, sigset_t* oldMask)` - funkcja służąca
do trwałej zmiany maski sygnałów blokowanych przez proces lub do odczytania jej biężacego stanu.
Decyduje, które sygnały będą czekały w kolejce. `how` definiuje, jak `mask` ma wpłynąć na bieżącą maskę:
    + `SIG_BLOCK` - dodajemy sygnały z `mask` do bieżącej maski blokowania
    + `SIG_UNBLOCK` - usuwamy te sygnały 
    + `SIG_SETMASK` - zastąpujemy bieżącą maskę blokowania dokładnie zbiorem podanym w `mask`.
```c
sigset_t mask, oldMask;
sigemptyset(&mask);
sigemptyset(&oldMask);

sigaddset(&mask, SIGUSR1);
sigaddset(&mask, SIGINT);

sigprocmask(SIG_BLOCK, &mask, &oldMask);
```

- `sigsuspend(const sigset_t* mask)` - funkcja **atomowa** do bezpiecznego czekania na sygnał. 
Jej działanie atomowe można podzielić na 3 części:
1) Zmiana maski blokującej. Tymczasowo ustawia maskę blokowania procesu na maskę podaną w argumencie `mask`.
Zwykle jest to maska, w której odblokowany jest oczekiwany sygnał.
2) Usypia wykonanie procesu i czeka na nadejście jakiegokolwiek sygnału, który nie jest 
blokowany przez tą nową maskę.
3) Gdy sygnał dotrze i zostanie obsłużony przez handler funkcja przywraca masję sygnałów do stanu,
w jakim była wywołaniem tej funkcji. 
Funkcja zawsze zwraca -1 i ustawia `errno` na `EINTR`.
Jest używana w celu zapobiegania *race condition*, w którym oczekiwany sygnał mógłby
nadejść pomiędzy odblokowaniem a wejściem do stanu uśpienia. Atomowość `sigsuspend()` 
sprawia, że nie istnieje tego małego okna czasowego pomiędzy dwoma instrukcjami. 
```c
// mask zawiera SIGUSR1, old_mask jest pusta
sigprocmask(SIG_BLOCK, &mask, &old_mask);

while (!sig_usr1_flag) {
    sigsuspend(&old_mask); 
}
```

## Różnica pomiędzy `wait` a `waitpid`

- funkcja `wait(int* status)` zawiesza działanie procesu rodzica do momentu, aż **którykolwiek** z jego procesów potomnych zakończy działanie. Do zmiennej `&status` wpada zakodowana liczba bitowa, która oznacza status, w którym dziecko się zakończyło. Status ten można sprawdzić za pomocą różnych makr:
    + `WIFEXITED(status)` - zwraca 1, jeśli dziecko zakończyło się normalnie poprzez `exit(0)` albo `return`
    + `WIFSIGNALED(status)` - zwraca 1, jeśli dziecko zostało zabite przez jakiś sygnał (np. `SIGKILL`)
    + `WTERMSIG(status)` - zwraca numer sygnału, który zabił dziecko.
Funkcja `wait` zwraca PID zakończonego dziecka. Jeśli zwróci -1, to oznacza, że zwróciła błąd `ECHILD`, który tak naprawdę nie oznacza błędu logiki, tylko, że nie ma więcej dzieci. A więc zwykle proces rodzica czeka na zakończenie wszystkich dzieci (jeśli jego nie obchodzi stan zakonczenia dzieci) poprzez pętlę:
```c
while (wait(NULL) > 0) {}
```
- funkcja `waitpid(pid_t pid, int* status, int options)` za pomocą użycia opcji `WNOHANG` służy do czekania na skończone dzieci BEZ blokady procesu rodzica. Czekać można na zakończenie konkretnego dziecka (pid > 0), na dowolne dziecko (pid = -1), na dzieci z tej samej grupy procesów (pid = 0). Do `&status` (podobnie jak w `wait`) wpada zakodowana liczba bitowa ze statusem zakończenia dziecka. Istnieje też zmienna `options` z opcjami:
    + `0` - domyślna. Blokuje proces rodzica, aż dziecko/dzieci zmienią stan. De facto to samo, co i zwykły `wait`
    + `WNOHANG` - **nieblokująca** - jeśli dziecko nie zmieniło stanu, zwraca '0' natychmiast. Używa się do sprawdzienia stanu w pętli głównej lub w handlerze sygnałów
    + `WUNTRACED` - reaguje także na dzieci, które zostały zatrzymane, a nie tylko zakończone.
    + `WCONTINUED` - reaguje także na dzieci, które zostały wznowione.
```c
void sigchld_handler(int sig) {
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0) {}
}
```

