# Wątki i muteksy - Notatka 

## Różnica pomiędzy wątkiem (thread) a procesem (process)
- **Proces** to samodzielna jednostka wykonawcza, posiada własną, **odizolowaną** przestrzeń adresową. **Wątek** to jest tzw. *lekki proces* (LWP), a mianowicie pojedynczy ciąg instrukcji wewnątrz procesu. *Wielowątkowość* - sytuacja, w której jeden proces ma wiele punktów kontrolnych (*flows of control*) i stanów wykonania, działających jednocześnie.
- **Sterta** (heap). Przechowuje to, co programista zaalokował ręcznie poprzez `malloc()`, `calloc()`, `realloc()` itd. Jest **współdzielona** dla wszystkich wątków w obrębie jednego procesu. To znaczy, że każdy wątek ma dostęp do tej pamięci. Wymaga to jednak synchronizacji (o tym trochę później).  
- **Dane globalne**. Zmienne zdefiniowane poza funkcjami (np. zmienne globalne) są **współdzielone**.
- **Otwarte pliki i deskryptory**.  Też są **współdzielone**. Jeśli wątek A otworzy jakiś plik (np. log.txt), to wątek B w obrębie tego samego będzie od tego momentu miał możliwość korzystania z tego otwartego deskryptora. UWAGA: zostanie współdzielony też wskaźnik *offset*.  Zatem wymaga to też synchronizacji, aby zapobiec `race condition`. 
- **.text**. Kod źródłowy programu, zawierający instrukcje maszynowe jest **współdzielony** dla wszystkich wątków. 

- **Stos**. Przechowuje zmienne lokalne itd. Jest **osobny** dla każdego wątku. 
- **Execution state**. Prcehowuje stan procesora: rejestry IP (Instruction Pointer), SP (Stack Pointer), BP (Break Pointer) itd. Jest **osobny** dla każdego wątku. 

### Różnica pomiędzy współbieżnością a równoległością
*Równoległość* - wiele zadań robi się fizycznie w tym samym momencie. *Współbieżność* - wiele zadań robi się w tym samym przedziale czasu, jednak niekoniecznie w tej samej chwili (Context-Switch).

## Wątki. Podstawowe zagadnienia
- Biblioteka `<pthread.h>`, zawiera zestaw funkcji, flag, struktur itd. do pracy z wątkami. (Uwaga: model 1-to-1 obsługi wątków przez jądro).
- `int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg)` - funkcja do tworzenia nowego wątku:
    + *thread* - wskaźnik do zmiennej typu `pthread_t`, gdzie zostanie zapisane ID nowo stworzonego wątku;
    + *attr* - wskaźnik do struktury typu `pthread_attr_t` z atrybutami wątku (np. rozmiar stosu, parametr joinable itd.), `NULL` ustawia wartości domyślne;
    + **start_routine* - wskaźnik na funkcję, która przyjmuje `void*` i zwraca też `void*`. Tutaj podajemy funkcję, którą definiujemy przed main'em. Wewnątrz tej funkcji będzie zdefiniowana praca wątku. 
    + *arg* - wskaźnik do zmiennej typu `void`, przyjmujący argumenty przekazywane do funkcji `start_routine`.
- `int pthread_join(pthread_t thread, void** retval)` - funkcja do oczekiwania na zakończenie wątku:
    + *thread* - ID wątku, na który czekamy;
    + *retval* - wskaźnik do zmiennej typu `void*`, gdzie zostanie zapisany wynik zwrócony przez wątek (wartość zwrócona przez funkcję `start_routine`).
    UWAGA: joinować można tylko wątki, które zostały stworzone jako *joinable* (domyślnie tak jest). Wątki *detached* nie mogą być joinowane. Joinować wątek może nie tylko wątek główny, ale też inne wątki.
- `pthread_attr_init(pthread_attr_t* attr)` - inicjalizuje strukturę atrybutów wątku z wartościami domyślnymi.
- `pthread_attr_setstack(pthread_attr_t* attr, void* stack_ptr, size_t stack_size)` - ustawia adres i rozmiar stosu dla wątku. `stack_ptr` to wskaźnik do pamięci zaalokowanej przez programistę (np. poprzez `malloc()`), a `stack_size` to rozmiar tej pamięci.
- `pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size)` - ustawia tylko rozmiar stosu dla wątku. Stos zostanie zaalokowany automatycznie przez system.
- `pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state)` - ustawia stan odłączenia wątku. `detach_state` może przyjmować wartości `PTHREAD_CREATE_JOINABLE` (domyślnie) lub `PTHREAD_CREATE_DETACHED`. Jeśli wątek jest DETACHED, to nie można go joinować. Wtedy nie można też pobrać wartości zwróconej przez wątek. Zasoby systemowe są zwalniane automatycznie po zakończeniu wątku przez system.
- `int pthread_detach(pthread_t thread)` - odłącza wątek, który został wcześniej stworzony jako JOINABLE. Po wywołaniu tej funkcji wątek staje się DETACHED. Nie można go już joinować, nie da sie pobrać wartości zwróconej przez wątek i nie da się z powrotem zmienić na JOINABLE. Jest to operacja jednokierunkowa.
- `void pthread_exit(void* retval)` - kończy działanie wątku i zwraca wartość `retval` do wątku, który go joinował. Jeśli wątek główny wywoła tę funkcję, to cały proces zostanie zakończony (wszystkie wątki zostaną zakończone).
- `pthread_self()` - zwraca ID wątku, który wywołał tę funkcję.
- `int pthread_equal(pthread_t t1, pthread_t t2)` - porównuje dwa ID wątków. Zwraca wartość różną od zera, jeśli są równe, lub zero, jeśli są różne.

### Sposoby zakończenia wątku
1. Zakończenie funkcji `start_routine` poprzez `return` - wartość zwrócona przez `return` jest przekazywana do wątku, który go joinował. Jest to równoważne z wywołaniem `pthread_exit(retval)`, gdzie `retval` to wartość zwrócona przez `return`.
2. Wywołanie `pthread_exit(void* retval)` - wartość `retval` jest przekazywana do wątku, który go joinował.
3. Zakończenie całego procesu poprzez `exit()` lub powrót z funkcji `main()` - wszystkie wątki zostają zakończone natychmiast, bez możliwości zwrócenia wartości do wątków joinujących.
4. Zakończenie wątku przez inny wątek za pomocą `pthread_cancel(pthread_t thread)` - wątek zostaje zakończony, ale nie ma możliwości zwrócenia wartości do wątku joinującego.
5. Nieobsłużony sygnał - jeśli wątek otrzyma sygnał, który nie jest obsługiwany, to wątek zostaje zakończony natychmiast. Np. `SIGSEGV`, `SIGFPE` itd.

### JOINABLE vs DETACHED
Jeżeli nie obchodzi nas wynik działania wątku, to lepiej jest stworzyć wątek jako DETACHED. Wtedy system automatycznie zwolni zasoby po zakończeniu wątku. Wątki JOINABLE wymagają jawnego joinowania, co może prowadzić do wycieków pamięci, jeśli o tym zapomnimy. Warto jednak pamiętać, że wątki DETACHED nie mogą być joinowane i nie można pobrać ich wyniku. Nie można też zmienić wątku z DETACHED na JOINABLE.

### Anulowalność wątków
Wątek może kontrolować, jak reaguje na żądania anulowania (cancellation requests, np. `pthread_cancel()`) poprzez ustawienie dwóch atrybutów:
1. **Stan anulowalności (cancellation state)** - `pthread_setcancelstate(int state, int* oldstate)`:
- `PTHREAD_CANCEL_ENABLE` (domyślny) oznacza, że wątek może być anulowany. 
- `PTHREAD_CANCEL_DISABLE` oznacza, że wątek **nie** może być anulowany, jednak prośby nie znikają, tylko są odkładane do momentu, aż wątek zmieni stan na `PTHREAD_CANCEL_ENABLE` (są kolejkowane).
2. **Typ anulowalności (cancellation type)** - `pthread_setcanceltype(int type, int* oldtype)`:
- `PTHREAD_CANCEL_DEFERRED` (domyślny i bezpieczny) oznacza, że anulowanie nastąpi w określonych punktach anulowalności (cancellation points), takich jak `pthread_join()`, `pthread_cond_wait()`, `read()`, `write()`, `sleep()`, `pthread_testcancel()` itd.
- `PTHREAD_CANCEL_ASYNCHRONOUS` oznacza, że wątek może być anulowany w dowolnym momencie. Jest to niebezpieczne, ponieważ może prowadzić do niespójnego stanu danych współdzielonych.

### Problemy z współbieżnym dostępem do zasobów współdzielonych
Wątki w obrębie jednego procesu współdzielą pamięć (sterta, dane globalne, otwarte pliki itd.). Może to prowadzić do problemów, takich jak np. **race condition** (wyścig), gdzie dwa lub więcej wątków jednocześnie modyfikuje te same dane, co prowadzi do niespójnego stanu. Aby temu zapobiec, stosuje się mechanizmy synchronizacji, takie jak **muteksy**.
Brak synchronizacji nie prowadzi do błędów logicznych, tylko w przypadku gdy mamy kilka wątków read-only. Jeśli mamy chociaż jeden wątek, który modyfikuje dane, to musimy stosować synchronizację.

### Clean-up handlers
Clean-up handlers to funkcje, które są wywoływane automatycznie podczas anulowania wątku lub gdy wątek kończy się poprzez `pthread_exit()`. Służą do zwalniania zasobów, takich jak muteksy, pamięć itd.
- `void pthread_cleanup_push(void (*routine)(void*), void* arg)` - rejestruje funkcję clean-up handler `routine`, która przyjmuje argument `arg`. Funkcja ta zostanie wywołana, gdy wątek zostanie anulowany lub zakończy się poprzez `pthread_exit()`.
- `void pthread_cleanup_pop(int execute)` - usuwa ostatnio zarejestrowany clean-up handler. Jeśli `execute` jest różne od zera, to funkcja clean-up handler zostanie wywołana natychmiast.
UWAGA: clean-up handlers są wywoływane w odwrotnej kolejności do ich rejestracji (LIFO).

Przykład użycia clean-up handlers:
```c
void cleanup_function(void* arg) 
{
    // Zwalnianie zasobów, np. odblokowanie muteksu
}
void* thread_function(void* arg) 
{
    pthread_cleanup_push(cleanup_function, arg);
    
    // Kod wątku
    
    pthread_cleanup_pop(1);
    return NULL;
}
```

## Muteksy (mutexes)
**Muteksy** - podstawowe narzędzie do synchronizacji wątków. Służą do zapewnienia wzajemnego wykluczania (mutual exclusion) podczas dostępu do zasobów współdzielonych. Rozwiązują problem race condition.
**Intuicja**: muteks to jak zamek na drzwiach do jakiegoś pokoju. Tylko jedna osoba (wątek) może wejść do pokoju (sekcji krytycznej) w danym momencie. Inne osoby muszą poczekać, aż pierwsza osoba wyjdzie i odda klucz (odblokuje muteks).
`pthread_mutex_t` - typ danych reprezentujący muteks.
**Przykład**:
1. *Zablokowanie* (lock). Wątek A wywołuje funkcję blokującą. Jeśli muteks jest wolny, A staje się jego właścicielem i kontynuuje swoje zadanie. 
2. *Oczekiwanie* (waiting). Jeśli w tym czasie wątek B próbuje zablokować ten sam muteks, to zostaje zablokowany i czeka w kolejce, aż A go odblokuje.
3. *Sekcja krytyczna* (critical section). Wątek A wykonuje operacje na zasobach współdzielonych, mając wyłączny dostęp. Nikt mu nie przeszkadza.
4. *Odblokowanie* (unlock). Po zakończeniu pracy w sekcji krytycznej, A odblokowuje muteks, zwalniając blokadę i pozwalając innym wątkom (np. B) na kontynuację.
5. *Przekazanie*. System budzi wątek B, który był zablokowany, i pozwala mu zablokować muteks i wejść do sekcji krytycznej.

WAŻNA ZASADA: Tylko właściciel muteksu (wątek, który go zablokował) może go odblokować. Próba odblokowania muteksu przez inny wątek prowadzi do błędu.

### Podstawowe funkcje do obsługi muteksów
- `int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr)` - inicjalizuje muteks. `attr` może być `NULL`, co ustawia domyślne atrybuty. `mutex` to wskaźnik do zmiennej typu `pthread_mutex_t`. Zwraca 0 w przypadku sukcesu, lub kod błędu.
- `int pthread_mutex_destroy(pthread_mutex_t* mutex)` - niszczy muteks i zwalnia zasoby. Muteks musi być odblokowany i nie może być używany przez żaden wątek w momencie niszczenia. Zwraca 0 w przypadku sukcesu, lub kod błędu. Jeśli muteks jest nadal zablokowany lub używany, zwraca błąd `EBUSY`.
- `int pthread_mutex_lock(pthread_mutex_t* mutex)` - blokuje muteks. Jeśli muteks jest już zablokowany przez inny wątek, to wywołujący wątek zostaje zablokowany i czeka, aż muteks zostanie odblokowany. Zwraca 0 w przypadku sukcesu, lub kod błędu.
- `int pthread_mutex_unlock(pthread_mutex_t* mutex)` - odblokowuje muteks. Tylko wątek, który zablokował muteks, może go odblokować. Zwraca 0 w przypadku sukcesu, lub kod błędu. Jeśli inny wątek próbuje odblokować muteks, zwraca błąd `EPERM`.
- `int pthread_mutex_trylock(pthread_mutex_t* mutex)` - próbuje zablokować muteks bez blokowania wątku. Jeśli muteks jest wolny, zostaje zablokowany i funkcja zwraca 0. Jeśli muteks jest już zablokowany, funkcja zwraca błąd `EBUSY` i wątek kontynuuje działanie bez blokowania.

### Przykład użycia muteksu
```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 3
#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

long counter = 0; // Zasób współdzielony poprzez wszystkie watki
pthread_mutex_t counter_mutex; // Muteks do synchronizacji dostępu do counter

void* increment_counter(void* arg)
{
    for (int i = 0; i < 1000000; i++)
    {
        pthread_mutex_lock(&counter_mutex); 
        counter++; // Sekcja krytyczna
        pthread_mutex_unlock(&counter_mutex); 
    }

    return (void*)counter; 
}

int main (int argc, char** argv)
{
    pthread_t* threads = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    if (!threads)
        ERR("Malloc failed");

    if(pthread_mutex_init(&counter_mutex, NULL))
        ERR("Mutex init failed");

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, increment_counter, NULL);
        if (threads[i] < 0)
            ERR("Thread creation failed");
    }

    int* counters_at_the_end = (int*)malloc(NUM_THREADS * sizeof(int));
    if (!counters_at_the_end)
        ERR("Malloc failed");

    for (int i = 0; i < NUM_THREADS; i++)
    {
        void* ret_val;
        if (pthread_join(threads[i], &ret_val))
            ERR("Thread join failed");
        counters_at_the_end[i] = (int)(long)ret_val;
    }

    printf("Final counter value: %ld\n", counter);
    for (int i = 0; i < NUM_THREADS; i++)
    {
        printf("Counter value from thread %d: %d\n", i, counters_at_the_end[i]);
    }

    pthread_mutex_destroy(&counter_mutex);
    free(threads);
    free(counters_at_the_end);

    return EXIT_SUCCESS;
}
```

### Konfiguracja Muteksów (Atrybuty)

Domyślnie (`NULL`) muteksy mogą powodować zakleszczenia przy ponownym zablokowaniu. Aby zmienić to zachowanie, używamy obiektu atrybutów `pthread_mutexattr_t`.

#### Procedura inicjalizacji
```c
pthread_mutex_t mtx;
pthread_mutexattr_t attr; 

// 1. Inicjalizacja atrybutów
pthread_mutexattr_init(&attr);

// 2. Ustawienie typu (np. na rekurencyjny lub z obsługą błędów)
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

// 3. Inicjalizacja muteksu z wykorzystaniem atrybutów
pthread_mutex_init(&mtx, &attr);

// 4. Sprzątanie atrybutów (nie są już potrzebne po stworzeniu muteksu)
pthread_mutexattr_destroy(&attr);
```

