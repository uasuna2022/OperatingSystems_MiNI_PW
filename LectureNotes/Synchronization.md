# Synchronizacja - notatka

**Synchronizacja** w rzeczywistości jest *komunikacją* między procesami lub wątkami w celu koordynacji ich działań. Problem jest w tym, że procesy lub wątki **nie** wiedzą, co w danej chwili robią inne procesy lub wątki, więc muszą jakoś się komunikować, aby uniknąć konfliktów i zapewnić poprawne działanie programu.

**Cel** synchronizacji - bezpieczny dostęp do współdzielonych zasobów (np. pamięci, plików) oraz koordynacja działań między procesami lub wątkami.

## Typowe schematy synchronizacji
- **Mutual Exclusion (Wykluczenie wzajemne)** - zapewnia, że tylko jeden proces lub wątek może uzyskać dostęp do współdzielonego zasobu w danym momencie (innymi słowy, wykonywać krytyczną sekcję kodu).
- **Awaiting Result (Oczekiwanie na wynik)** - jeden proces lub wątek czeka na zakończenie innego procesu lub wątku, aby uzyskać wynik jego działania.
- **Awaiting Condition (Oczekiwanie na warunek)** - proces lub wątek czeka, aż spełniony zostanie określony warunek, zanim będzie mógł kontynuować swoje działanie.
- **Lockstep (Bariera)** - grupa wątków lub procesów czeka, aż wszystkie zakończą określone swoje działania, zanim przejdą do następnego etapu.
- **Rate Limiting (Ograniczanie przepustowości)** - pozwala maksymalnie n wątkom lub procesom wejście do sekcji krytycznej jednocześnie.
- **Bounded Buffer (Bufor o ograniczonym rozmiarze)** - klasyczny problem producenta-konsumenta, gdzie producent dodaje dane do bufora, a konsument je z niego pobiera, z ograniczeniem na maksymalny rozmiar bufora.

## Typowe narzędzia synchronizacji
- **Mutex (Muteks)** - mechanizm synchronizacji zapewniający wykluczenie wzajemne.
- **Semaphore (Semafor)** - licznik, który blokuje wątek, gdy jego wartość wynosi 0, i odblokowuje go, gdy wartość jest większa niż 0.
- **Condition Variable (Zmienna warunkowa)** - pozwala wątkom czekać na określony warunek, aż inny wątek powiadomi je o zajściu tego warunku.
- **Read-Write Lock (Zamek do odczytu i zapisu)** - pozwala wielu wątkom na jednoczesny odczyt, ale tylko jednemu na zapis, zapewniając wykluczenie wzajemne między odczytami a zapisami.
- **Barriers & Latches (Bariery i zatrzaski)** - blokują wątki do momentu, aż określona ich liczba dotrze do wspólnego punktu synchronizacji w kodzie.
- **Futures (Przyszłości)** - obiekty obiecujące dostarczenie wyniku w przyszłości, umożliwiające wątkom oczekiwanie na wynik operacji asynchronicznej. Wątek blokuje się na nich tylko wtedy, gdy próbuje uzyskać wynik, który jeszcze nie jest dostępny.

## Semafory
Semafory są jednym z najstarszych i najbardziej podstawowych mechanizmów synchronizacji. Zostały wprowadzone przez E.W. Dijkstrę w latach 60. XX wieku.

**Semafor** - nieujemny licznik przechowujący wartość całkowitą. **NIE MAJĄ** właściciela, służą po prostu jako współdzielony licznik dostępny dla wielu wątków. 
Ma 2 podstawowe operacje:
- `wait()` - zmniejsza wartość semafora o 1. Jeśli wartość semafora wynosi 0, wątek wywołujący `wait()` zostaje zablokowany, aż inny wątek wywoła `post()`.
- `post()` - zwiększa wartość semafora o 1. Jeśli są zablokowane wątki czekające na semafor, jeden z nich zostaje odblokowany, a wartość licznika pozostaje niezmieniona (można to potraktować jako zwiększenie wartości semafora o 1 i natychmiastowe zmniejszenie jej o 1 przez odblokowany wątek).

To, że semafory nie mają właściciela, oznacza, że **dowolny** wątek może wywołać `post()`, nawet jeśli to inny wątek wywołał `wait()`. Dzięki temu semafory są bardzo elastyczne i mogą być używane do różnych celów synchronizacyjnych.

**Przykład**: wątki T1 i T2 współdzielą semafor S z początkową wartością 0. T2 potrzebuje danych wygenerowanych przez T1, więc T2 wywołuje `wait(S)`, co blokuje T2, ponieważ wartość S wynosi 0. Gdy T1 zakończy generowanie danych, wywołuje `post(S)`, co zwiększa wartość S do 1 i odblokowuje T2, pozwalając mu kontynuować działanie. 
W tym przykładzie T2 nie musi zwolnić semafora S po jego założeniu. T1 może wywołać `post(S)` niezależnie od tego, który wątek wywołał `wait(S)`.

Dzięki **semaforom** możemy też ograniczyć liczbę wątków mających jednoczesny dostęp do sekcji krytycznej. Np. jeśli chcemy pozwolić maksymalnie 3 wątków współdzielać zasoby, to inicjalizujemy semafor z wartością 3. Każdy wątek wywołuje `wait()` przed wejściem do sekcji krytycznej i `post()` po jej opuszczeniu. W ten sposób tylko 3 wątki mogą jednocześnie przebywać w sekcji krytycznej, a pozostałe muszą czekać, aż któryś z nich ją opuści.

### Przydatne funkcje semaforów
- `int sem_init(sem_t *sem, int pshared, unsigned int value)` - inicjalizuje semafor `sem` z wartością `value`. Jeśli `pshared` jest różne od 0, semafor może być współdzielony między procesami. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku. Jeśli korzystamy z opcji międzyprocesowej, musimy użyć pamięci współdzielonej do przechowywania semafora. Tylko tam możemy umieścić semafor, żeby był on dostępny dla wielu procesów. Funkcja ta tworzy semafor **anonimowy** i działa na procesach pokrewnych (utworzonych za pomocą `fork()`).
- `sem_t* sem = sem_open(const char *name, int oflag, mode_t mode, unsigned int value)` - tworzy lub otwiera nazwany semafor o nazwie `name` z wartością `value`. Jeśli semafor o podanej nazwie już istnieje, funkcja go otwiera. Zwraca wskaźnik do semafora w przypadku sukcesu, `SEM_FAILED` w przeciwnym przypadku. Nazwane semafory mogą być używane przez dowolne procesy w systemie, pod warunkiem, że znają jego nazwę. Procesy tutaj już NIE muszą być pokrewne.
- `int sem_destroy(sem_t *sem)` - niszczy semafor `sem`, zwalniając zasoby z nim związane. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku. Należy pamiętać, że semafor nie może być zniszczony, jeśli są do niego przypisane jakieś wątki lub procesy. (Dla semaforów anonimowych).
- `int sem_close(sem_t *sem)` - zamknięcie nazwanego semafora. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku. Nie usuwa to jednak semafora z systemu - do tego służy `sem_unlink()`. (Dla semaforów nazwanych).
- `int sem_wait(sem_t *sem)` - wykonuje operację `wait()` na semaforze `sem`. Zmniejsza wartość semafora o 1 lub blokuje wątek, jeśli wartość wynosi 0. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku.
- `int sem_post(sem_t *sem)` - wykonuje operację `post()` na semaforze `sem`. Zwiększa wartość semafora o 1 lub odblokowuje jeden z oczekujących wątków, jeśli taki istnieje. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku.
- `int sem_getvalue(sem_t *sem, int *sval)` - pobiera aktualną wartość semafora `sem` i zapisuje ją w `sval`. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku. Należy pamiętać, że wartość ta może być nieaktualna, jeśli inne wątki lub procesy modyfikują semafor równocześnie, zatem nie zaleca się używania tej funkcji do podejmowania decyzji synchronizacyjnych.
-  `int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)` - podobna do `sem_wait()`, ale z dodatkowym parametrem `abs_timeout`, który określa absolutny czas, po którym operacja `wait()` zakończy się niepowodzeniem, jeśli semafor nie stanie się dostępny. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku (np. `ETIMEDOUT`, jeśli operacja przekroczyła limit czasu).
- `int sem_trywait(sem_t *sem)` - podobna do `sem_wait()`, ale nie blokuje wątku, jeśli semafor nie jest dostępny. Zamiast tego zwraca kod błędu `EAGAIN`, jeśli wartość semafora wynosi 0. Zwraca 0 w przypadku sukcesu, kod błędu w przeciwnym przypadku.

Warto pamiętać, że **NIE można** kopiować struktur synchronizacyjnych. Zawsze musimy posługiwać się wskaźnikami do nich. Kopiowanie struktur może prowadzić do nieprzewidywalnych zachowań i błędów w synchronizacji.

```c
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s, line nr. %d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void* prepare_data (void* arg)
{
    sem_t* sem = (sem_t*)arg;

    printf("[%ld] I'm preparing some data for others...\n", pthread_self());
    sleep(3);
    printf("[%ld] Data is ready!\n", pthread_self());

    if (sem_post(sem))
        ERR("sem_post");
    
    return NULL;
}

void* do_some_job (void* arg)
{
    sem_t* sem = (sem_t*)arg;

    printf("[%ld] I'm waiting for data to be prepared...\n", pthread_self());
    if (sem_wait(sem))
        ERR("sem_wait");
    printf("[%ld] Data received! Now I can do my job!\n", pthread_self());
    sleep (2);
    printf("[%ld] Job done!\n", pthread_self());

    return NULL;
}

int main()
{
    sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
    if (sem_init(sem, 0, 0))
        ERR("sem_init");
    
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, prepare_data, sem))
        ERR("pthread_create");
    if (pthread_create(&t2, NULL, do_some_job, sem))
        ERR("pthread_create");

    if (pthread_join(t1, NULL))
        ERR("pthread_join");
    if (pthread_join(t2, NULL))
        ERR("pthread_join");
    
    if (sem_destroy(sem))
        ERR("sem_destroy");
    
    free(sem);
    return EXIT_SUCCESS;
}
```