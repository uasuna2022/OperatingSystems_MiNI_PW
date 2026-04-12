# Pamięć współdzielona 

## `mmap()`

Funkcja `mmap()` pozwala na stworzenie wskaźnika do obszaru pamięci, który jest zaalokowany w RAM-ie. Można go użyć do stworzenia pamięci **współdzielonej** między procesami. 

Jeśli proces rodzic utworzy taki wskaźnik do pewnej pamięci współdzielonej, a następnie wywoła `fork()`, to proces potomny będzie miał dostęp do tej samej pamięci, co umożliwia komunikację między procesami. 

W przeciwieńswtie do funkcji `malloc()`, która alokuje pamięć na stercie procesu, w wyniku czego inny proces nie ma do niej dostępu, `mmap()` alokuje pamięć w specjalnym obszare jądra dla obiektów IPC, co umożliwia współdzielenie tej pamięci między procesami.


### Składowe funkcji

```c
void* mmap (void* addr, size_t length, int prot, int flags, int fd, off_t offset);
```

- `addr` - wskazuje na adres, pod którym ma zostać zaalokowana pamięć. Jeśli jest ustawiony na `NULL`, system wybierze odpowiedni adres (domyślne zachowanie).
- `length` - określa rozmiar pamięci do zaalokowania w bajtach.
- `prot` - określa prawa dostępu do pamięci (trochę podobnie do uprawnień do plików):
    + `PROT_READ` - pamięć może być odczytywana
    + `PROT_WRITE` - pamięć może być zapisywana
    + `PROT_EXEC` - pamięć może być wykonywana
    + `PROT_NONE` - brak dostępu do pamięci
- `flags` - określa sposób mapowania pamięci:
    + `MAP_SHARED` - pamięć jest współdzielona między procesami
    + `MAP_PRIVATE` - pamięć jest prywatna dla procesu (zmiany nie są widoczne dla innych procesów)
    + `MAP_ANONYMOUS` - pamięć nie jest powiązana z żadnym plikiem (nie wymaga deskryptora pliku, ustawia `fd` na -1). Brak tej flagi oznacza, że pamięć będzie mapowana do pliku, którego deskryptor jest podany w `fd`. Po zakończeniu wszystkich procesów korzystających z tej pamięci, system automatycznie zwolni zasoby, a zawartość pamięci zostanie utracona.
- `fd` - deskryptor pliku, który ma być mapowany do pamięci
- `offset` - przesunięcie w pliku, od którego ma zostać zmapowana pamięć (dla `MAP_ANONYMOUS` powinno być ustawione na 0).

Funkcja `mmap()` zwraca wskaźnik do zaalokowanej pamięci lub `MAP_FAILED` w przypadku błędu.

### Inne przydatne do `mmap()` funkcje

- `munmap(void* addr, size_t length)` - zwalnia pamięć zaalokowaną przez `mmap()`. Należy podać adres i rozmiar pamięci do zwolnienia. Jeśli `length` jest mniejszy niż rozmiar zaalokowanej pamięci, tylko część pamięci zostanie zwolniona, a reszta pozostanie dostępna. Np. jeśli zaalokowaliśmy 100 bajtów, a `munmap()` wywołamy z `length` równym 50, to tylko pierwsze 50 bajtów zostanie zwolnione, a pozostałe 50 bajtów nadal będzie dostępne. Można podać `addr + 50` jako adres, aby zwolnić drugą część pamięci.
- `msync(void* addr, size_t length, int flags)` - synchronizuje zawartość pamięci z dyskiem. Jest to przydatne, gdy pamięć jest mapowana do pliku, a zmiany w pamięci mają być zapisane na dysku. Należy podać adres i rozmiar pamięci do synchronizacji oraz flagi określające sposób synchronizacji (np. `MS_SYNC` - synchronizacja blokująca, `MS_ASYNC` - synchronizacja asynchroniczna). Jest to funkcja analogiczna do `fflush()` dla plików. `msync()` jest szczególnie ważna, gdy używamy `MAP_SHARED`, ponieważ zmiany w pamięci muszą być zapisane na dysku, aby były widoczne dla innych procesów. Jeśli używamy `MAP_PRIVATE`, zmiany w pamięci nie są zapisywane na dysku, więc `msync()` nie jest potrzebna.


## Ważna informacja

- Kiedy mapujemy pamięć do jakiegoś procesu, to mapuje się cała strona pamięci (zazwycaj 4096 bajtów). Oznacza to, że nawet jeśli proces pracuje tylko na kilkudziesięciu bajtach, to nadal cały obszar strony jest mapowany do pamięci. A więc musimy na poziomie kodu kontrolować, żeby przypadkiem proces A nie wyszedł poza zakres swojej sekcji i nie nadpisał danych procesu B. Z poziomu OS nie będzie to błędem SIGSEGV (Segmentation Fault), iż proces A będzie miał dostęp do całej strony. Poza tym nie można ustalić offset, który nie będzie krotnością rozmiaru strony. Np. próba ustalenia `offset` na wartość 100, otrzymamy błąd `EINVAL` (Invalid argument). 

- Po zakończeniu procesu dane śmieciowe (np. te bajty 100-4095) zostają wyczyszczone, a więc potem mogą być ponownie użyte przez inny proces. Jednakże, te pierwsze 100 bajtów, które są rzeczywistym plikiem stworozonym po `shm_open()`, nadal będą zawierać dane, które zostały zapisane przez proces A. Dane leżą w sekcji `/dev/shm/` i są dostępne dla innych procesów, które mogą je odczytać (szczegóły później). Katalog `/dev/shm/` zostaje wyczyszczony po restarcie systemu lub po `shm_unlink()`, ale dopóki tego nie zrobimy, dane pozostają dostępne.

## Obiekty nazwane pamięci współdzielonej (`shm_open()`)

`mmap()` tworzyło pamięć współdzieloną, ale nie była ona powiązana z żadnym plikiem. Korzystać z tej pamięci zatem mogły tylko procesy pokrewnione (np. rodzic i dziecko). Z powodu braku nazwy, inne procesy nie mogły się do niej odwołać. 

Funkcja `shm_open()` pozwala na stworzenie obiektu pamięci współdzielonej, który jest powiązany z nazwą. Dzięki temu, różne procesy mogą odwoływać się do tej samej pamięci współdzielonej za pomocą tej nazwy, nawet jeśli nie są ze sobą spokrewnione.

```c
int shm_open(const char* name, int oflag, mode_t mode);
```

- `name` - nazwa obiektu pamięci współdzielonej. Musi zaczynać się od znaku '/' i nie może zawierać innych znaków '/' (np. `/my_shared_memory` jest poprawne, ale `/my/shared/memory` jest niepoprawne). Nazwa ta jest używana do identyfikacji obiektu pamięci współdzielonej w systemie.

- `oflag` - flagi określające sposób otwarcia obiektu pamięci współdzielonej. Mogą to być:
    + `O_CREAT` - tworzy nowy obiekt pamięci współdzielonej, jeśli nie istnieje
    + `O_EXCL` - w połączeniu z `O_CREAT`, powoduje błąd, jeśli obiekt już istnieje
    + `O_RDWR` - otwiera obiekt do odczytu i zapisu
    + `O_RDONLY` - otwiera obiekt tylko do odczytu

- `mode` - określa uprawnienia do obiektu pamięci współdzielonej (podobnie jak uprawnienia do plików). Np. `0666` oznacza, że wszyscy użytkownicy mają prawo do odczytu i zapisu.

Logika jest bardzo podobna do zwykłego `open()`, ale zamiast tworzyć/otwierać plik, tworzy/otwiera obiekt pamięci współdzielonej. Po wywołaniu `shm_open()`, otrzymujemy deskryptor pliku, który można następnie użyć z `mmap()` do mapowania tej pamięci do przestrzeni adresowej procesu.

Stworzony obiekt ma rozmiar 0 bajtów, a więc przed użyciem należy skrozystać z funkcji `ftruncate(fd, size)` do ustawienia rozmiaru obiektu pamięci współdzielonej. 

Po zakońćzeniu pracy z obiektem nalezy go zamknąć za pomocą `close(fd)` i usunąć z systemu za pomocą `shm_unlink(name)`, aby zwolnić zasoby.

## Współdzielenie obiektów synchronizacji. Robust mutexy

Mając pamięć współdzieloną, możemy również współdzielić obiekty synchronizacji, takie jak mutexy, semafory czy zmienne warunkowe. 

- **Semafory**. Semafory mogą być współdzielone między procesami, jeśli drugi parametr funkcji `sem_init()` - `pshared` - jest ustawiony na inną wartość, niż 0. Wystarczy wtedy umieścić semafor w pamięci współdzielonej, wszystkie procesy będą miały do niego dostęp i mogły go używać do synchronizacji. 

- **Mutexy**. Mutexy też mogą byś współdzielone między procesami, jednak wymagają większej ostrożności. Potrzebujemy ustalić atrybut `pthread_mutexattr_t` na `pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)`, aby wskazać, że mutex będzie współdzielony między procesami. Następnie umieszczamy ten mutex w pamięci współdzielonej, aby wszystkie procesy miały do niego dostęp.
Muteksy jednak mają być odblokowane w tym samym procesie, w którym zostały zablokowane. Jeśli proces A zablokuje mutex, a następnie zakończy się bez odblokowania go, to proces B, który będzie próbował zablokować ten sam mutex, zostanie zablokowany na zawsze. Aby temu zapobiec, można użyć tzw. **robust mutexów**. Robust mutexy są specjalnym rodzajem mutexów, które pozwalają innym procesom wykryć, że właściciel mutexu zakończył się bez odblokowania go. W takim przypadku, inny proces może przejąć kontrolę nad tym mutexem i odblokować go, aby umożliwić dalszą pracę. Aby stworzyć robust mutex, należy ustawić atrybut `pthread_mutexattr_t` na `pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST)`. W ten sposób, jeśli proces A zablokuje robust mutex i zakończy się bez odblokowania go, to proces B, który będzie próbował zablokować ten sam mutex, otrzyma błąd `EOWNERDEAD`, co oznacza, że właściciel mutexu jest martwy. Proces B może wtedy wywołać `pthread_mutex_consistent(&mutex)`, aby przejąć kontrolę nad tym mutexem i odblokować go, umożliwiając dalszą pracę. Robust mutexy są szczególnie przydatne w sytuacjach, gdzie istnieje ryzyko, że procesy mogą zakończyć się niespodziewanie, co może prowadzić do zablokowania innych procesów, które próbują uzyskać dostęp do tego samego zasobu.

- **Zmienne warunkowe i bariery** też posiadają specjalne funkcje `pthread_condattr_setpshared()` i `pthread_barrierattr_setpshared()`, które pozwalają na ich współdzielenie między procesami.

