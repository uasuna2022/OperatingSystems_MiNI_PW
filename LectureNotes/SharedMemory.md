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