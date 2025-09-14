# Systemy Operacyjne 1 - Wykład 2 - System plików API

## Rola systemu operacyjnego w kontekście systemu plików
System operacyjny pełni 3 główne funkcje związane z obsługą plików:
- Definiuję logiczny abstrakcyjny model plików (czyli odpowiada na pytania:)
    - co to jest plik?
    - jak wygląda jego zawartość?
    - jakie ma atrybuty i czym są atrybuty (rozmiar, właściciel, czas modyfikacji)
    - czy dane są strukturyzowane? Jeżeli tak, to w jaki sposób? 
- Udostępnia interfejs systemowy (syscalls)
    - `open()`, `read()`, `write()`, `close()` - podstawowe operacje I/O
    - `unlink()`, `mkdir()`, `link()`, `stat()` - operacje na plikach i katalogach
- Implementacja tego modelu fizycznie
    - mapowanie abstrakcyjnego modelu na fizyczne danena dysku
    - efektywne, bezpiecznie i różnorodne operacje na różnych typach sprzętu (HDD, SSD itd.)

## Czym jest abstrakcyjny plik?
Z punktu widzenia OS, **plik** - zwykły **ciąg bajtów**. System operacyjny nie interesuje się strukturą danych wewnątrz pliku (zależy ona od aplikacji).
Możliwe struktury danych w pliku:
- brak struktury (surowe bajty, np. `file.bin`)
```
0x00, 0x01, 0x0d, ...
```
- rekordy stałej długości (np. rekord z dowolnej bazy danych)
```
record 1: [32 bajty]
record 2: [32 bajty]
...
```
- rekordy zmiennej długości (np. linie tekstu, plik `.txt`)
```
line 1: "jfhdskjfhs"
line 2: "xddd"
line 3: "ilmawy"
```

## Dostęp do danych: sekwencyjny vs losowy

### Dostęp sekwencyjny (`read()`)

* Pozycja w pliku jest utrzymywana przez system operacyjny.
* Każde wywołanie `read()` odczytuje dane **od aktualnej pozycji** i **przesuwa wskaźnik dalej**.
* Przykład:

  ```c
  int fd = open("plik.txt", O_RDONLY);
  char buf[10];

  ssize_t nbyte = read(fd, buf, 10);  // czyta bajty [0..9]
  ssize_t nbyte2 = read(fd, buf, 10);  // czyta bajty [10..19]
  // nbyte i nbyte2 zawierają informację, ile dokładnie bajtów zostało wczytane (0 - dotarliśmy do końca pliku)
  ```
* Użyteczne do czytania pliku "od początku do końca"

### Dostęp losowy (`pread()`)

* Podaje się **konkretna pozycja (offset)** skąd chcemy odczytać zawartość.
* **Nie zmienia** systemowego wskaźnika pliku.
* Przykład:

  ```c
  int fd = open("plik.txt", O_RDONLY);
  char buf[10];

  pread(fd, buf, 10, 0);   // czyta bajty [0..9]
  pread(fd, buf, 10, 100); // czyta bajty [100..109]
  ```
* Użyteczne np. w bazach danych, przy wielu wątkach, przy aktualizacji fragmentów pliku

### Sprawdzanie i ustawianie pozycji przy `read()`:

* Można użyć `lseek()` do:
  * sprawdzenia pozycji: `lseek(fd, 0, SEEK_CUR);`
  * ustawienia pozycji: `lseek(fd, 0, SEEK_SET);`

### Porównanie:

| Cecha                       | `read()` | `pread()` |
| --------------------------- | -------- | --------- |
| Używa systemowej pozycji    | ✅ Tak    | ❌ Nie     |
| Przesuwa wskaźnik pliku     | ✅ Tak    | ❌ Nie     |
| Bezpieczne dla wielu wątków | ❌ Nie    | ✅ Tak     |
| Wymaga `lseek()`?           | ✅ Czasem | ❌ Nie     |

### Przykład działania:

```c
char buf[11];

int fd = open("test.txt", O_RDONLY);
read(fd, buf, 10);   // czyta od początku
buf[10] = '\0';
printf("Sekwencyjnie: %s\n", buf);

pread(fd, buf, 10, 5); // czyta od pozycji 5 niezależnie od poprzedniego read
buf[10] = '\0';
printf("Losowo od 5: %s\n", buf);
```

---

## Atrybuty plików

Każdy plik w systemie ma więcej niż tylko dane bajtowe:

- **name** – nazwa pliku (etykieta w katalogu)
- **identifier** – unikalny ID, niezmienny przy zmianie nazwy
- **type** – np. zwykły plik, katalog, urządzenie znakowe
- **location** – fizyczna lokalizacja na urządzeniu
- **size** – rozmiar logiczny (ilość bajtów) i fizyczny (liczba bloków)
- **protection** – prawa dostępu i właściciel
- **timestamps** – daty utworzenia, modyfikacji, dostępu

Wszystkie te informacje są przechowywane w strukturze `inode`, która poza tym zawiera wskaźniki do bloków danych na dysku, gdzie zapisana jest zawartość pliku. Nie zawiera jednak bezpośrednio danych, nie zawiera ścieżki dostępu i nazwy pliku (nazwa przechowywana w kataloga i "linkowana" do inoda).

---

## Struktury katalogów

- **Flat** – wszystkie pliki w jednym katalogu (może wystąpić konflikt nazw - żadne 2 pliki muszą mieć różne nazwy)
- **Two-level** – osobne katalogi dla każdego użytkownika (2 pliki z różnych katalogów mogą mieć te same nazwy bez konfliktu)
- **Multi-level** (struktura drzewiasta) – hierarchiczna struktura katalogów i podkatalogów - najczęściej używana (`/home/user/docs/plik.txt`)
- **Acyclic Graph** – twarde linki do plików (wiele ścieżek do jednego pliku), katalogi nie mogą mieć wielu linków

### Trochę o twardych linkach (hardlinks)

**Hardlink** - dodatkowy wpis w katalogu, który wskazuje dokładnie na ten sam inode, a więc na te same dane i metadane co i *oryginalny* plik. W wyniku jeden plik (a dokładnie jeden inode) może mieć wiele nazw/ścieżek wewnątrz tego samego systemu plików. 

Przykład:
```bash
echo "Hello from a!" > a.txt  # tworzymy zwykły plik i wpisujemy tam tekst 
ln a.txt b.txt    # tworzymy hardlink do oryginalnego pliku o nazwie b.txt
cat b.txt    # wynikiem będzie "Hello from a!", bo b.txt wskazuje na ten sam inode
```
Kilka uwag:
- jeśli istnieje więcej niż jeden link do jednego inode'a, to dopóki wszystkie one nie będą usunięte, odpowiednia pamięć (z danymi) nie zostanie fizycznie zwolniona
- podczas tworzenia pliku (`touch file.txt`) automatycznie tworzy się hardlink do niego
- czyli de facto usunięcie pliku (`rm file.txt`) nie oznacza, że zwalniamy pamięć odpowiadającą za przechowywanie zawartości tego pliku. Usuwa się tylko odpowiedni hardlink, i tylko potem jeśli `link count = 0` i plik nie jest otwarty przez żaden proces (czyli jeśli nikt nie ma deskryptora `fd` do niego), odbywa się zwolnienie bloków danych, czyli dane fizycznie znikają z dysku.

### Współdzielenie plików (Sharing semantics)

W systemie wielozadaniowym wiele procesów może jednocześnie czytać i zapisywać ten sam plik.  
Pojawia się wtedy pytanie: **co faktycznie zobaczy czytelnik (read), jeśli w tym samym czasie kilku autorów (write) zmienia zawartość?**

#### Możliwe strategie systemu operacyjnego:
- **Zakaz równoczesnego dostępu** – najprostsze, ale nieefektywne.
- **Dopuszczenie dostępu z ochroną przed uszkodzeniem FS** – system zapewnia integralność systemu plików, ale dane mogą być niespójne.
- **Widoczność zapisów**:
  - zmiany mogą być widoczne **od razu**,
  - albo **dopiero po zamknięciu pliku** (`close()`, `fsync()`).

#### Problem:
Bez synchronizacji może dojść do tzw. *race condition* – wynik odczytu jest nieprzewidywalny.

#### Rozwiązanie:
Stosuje się mechanizmy blokowania plików:
- `flock()` lub `fcntl()` w POSIX
- własne mechanizmy IPC (np. mutexy, semafory)

Przykład problematycznej sytuacji:
- Proces A pisze 100 bajtów,
- Proces B jednocześnie nadpisuje 20 bajtów w tym samym miejscu,
- Proces C próbuje czytać -> wynik nieokreślony.

## Deskryptory (file descriptors)

**File descriptor (FD)** to liczba całkowita (int), którą system operacyjny przydziela procesowi przy otwarciu pliku.  
Nie zawiera danych pliku, ale jest **uchwytem** wskazującym do struktur jądra, które opisują otwartą sesję plikową.
- Każdy proces ma własną **tablicę deskryptorów plików**. Indeks w tej tablicy jest właśnie numerem deskryptora (`fd`).
- Wpis tablicy wskazuje na systemową strukturę **open file table entry**, gdzie przechowywane są różne dane (wskaźnik do inode'a, aktualna pozycja `offset`, tryb dostępu, informacje o blokadach itd.)

### Standardowe deskryptory 
Każdy proces od startu defaultowo ma 3 deskryptory:
- `0` - **stdin** (standardowe wejście) - zazwyczaj klawiatura, można zmienić na dane z pliku za pomocą komendy:
```bash
./program < input.txt
```
- `1` - **stdout** (standardowe wyjście) - zazwyczaj terminal lub ekran, można zmienić na dowolny plik za pomocą komendy:
```bash
./program > output.txt
```
- `2` - **stderr** (standard error) - błędy, mają osobny kanał od stdout, zazwyczaj też ekran lub terminal, ale można zmienić na dowolny plik za pomocą komendy:
```bash
./program 2> errors.log
```

### Tworzenie deskryptora
Deskryptor tworzony jest przez wywołanie funkcji systemowej `open()`
```c
int fd = open("file.txt", O_RDONLY);
```
Sukces -> fd przyjmuje **najmniejszy wolny deskryptor** (np. 3, jeśli żadnych innych nie ma, bo 0, 1, 2 są zajęte);
Błąd -> fd przyjmuje wartość `-1` i ustawia odpowiednio zmienną `errno`.
Jeśli plik został zamknięty przez `close(fd)`, deskryptor znika i jego numer zwalnia się, a więc może być użyty ponownie przez następne `open()`.

#### Flagi 
Flagi dostępu:
- `O_RDONLY` - otworzenie pliku **tylko do odczytu**
- `O_WRONLY` - otworzenie pliku **tylko do zapisu**
- `O_RDWR` - otworzenie pliku **zarówno do zapisu, jak i do odczytu**
Flagi dodatkowe:
- `O_CREAT` - tworzenie pliku w przypadku jego braku (wymaga dodatkowego argumentu w `open()` - uprawnień)
- `O_EXCL` - używa się razem z `O_CREAT` i jeśli plik już istnieje, to zwróci się błąd `EEXIST`
- `O_TRUNC` - jeśli plik istnieje, to zostanie wyczyszczony całkiem
- `O_APPEND` - wszystkie zapisy do pliku dodawane są na końcu pliku 
- `O_NONBLOCK` - otworzenie pliku w trybie nieblokującym (przydatne dla FIFO i socketów)
- `O_SYNC` - wymusza zapis synchroniczny

### Usuwanie deskryptora
Deskryptor istnieje **dopóki proces działa** lub dopóki wywoła się `close(fd)`. Jeśli proces zakończy się bez `close()`, to system **automatycznie zamyka wszystkie jego deskryptory**.

### Podgląd deskryptorów
```bash
ls -l /proc/$$/fd # $$ - zmienna zawierająca pid obecnego procesu (echo $$)
ls -l /proc/2137/fd # deskryptory procesu z pid=2137
```

### Przykładowy kod
```c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main ()
{
    int fd = open("test1.txt", O_RDONLY, 0644);
    printf("%d - number of descriptor\n", fd);
    int pid = getpid();
    printf("%d - current pid\n", pid);
    sleep(5);
    close(fd);
    return 0;
}
```

## Scatter-gather I/O syscalls (readv() i writev())
Używa się ich, kiedy dane w pamięci są rozproszone (znajdują się w kilku osobnych fragmentach). Wtedy np. zamiast pisać kilka razy `write()` można użyć jednego `writev()`.

#### Struktura `iovec` (I/O vector)
```c
struct iovec {
    void *iov_base; // wskaźnik na fragment pamięci
    size_t iov_len; // długość tego fragmentu
};
```
#### Przykład użycia
Załóżmy, że mamy trzy osobne fragmenty tekstu w pamięci: "My name", "is", "Maksim!" Chcemy wpisać te fragmenty do konkretnego pliku, jednak nie chcemy robić 3 razy `write()`.
```c
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main()
{
    char fragment1[] = "My name ";
    char fragment2[] = "is ";
    char fragment3[] = "Maksim!\n";

    int fd = open("new_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    struct iovec iov[3]; // tworzymy tablice struktur typu iovec
    
    // Przypisujemy odpowiednie kawałki tekstu do odpowiednich struktur tablicy
    iov[0].iov_base = fragment1; 
    iov[0].iov_len = 8;
    iov[1].iov_base = fragment2;
    iov[1].iov_len = 3;
    iov[2].iov_base = fragment3;
    iov[2].iov_len = 8;

    if (writev(fd, iov, 3) == -1) // Zapisujemy cały tekst w jednym syscallu
    {
        perror("writev");
    }

    close(fd);
    return 0;
}

```

## Pobieranie atrybutów plików: `stat()`, `lstat()`, `fstatat()`

Funkcje te pozwalają pobierać informacje o pliku ze struktury **inode** (rozmiar, prawa dostępu, liczba linków, właściciel itp.)

### Różnice:
- **`stat(path, buf)`** – pobiera informacje o pliku wskazanym przez `path` (**śledzi symlinki**).
- **`lstat(path, buf)`** – pobiera informacje o samym pliku/odwołaniu (**nie śledzi symlinków**).
- **`fstatat(fd, path, buf, flag)`** – bardziej elastyczna wersja, pozwala określić katalog bazowy przez deskryptor `fd`; przydatna w sandboxach i przy pracy z symlinkami.

### Struktura `stat` (najważniejsze jej pola)
- `st_size` – rozmiar pliku w bajtach
- `st_mode` – typ pliku i prawa dostępu
- `st_nlink` – liczba hardlinków
- `st_uid` / `st_gid` – właściciel i grupa
- `st_atime`, `st_mtime`, `st_ctime` – czasy dostępu, modyfikacji i zmiany metadanych

### Przykład
```c
#include <stdio.h>
#include <sys/stat.h>

int main() {
    struct stat sb;

    if (stat("plik.txt", &sb) == 0) {
        printf("File size: %ld bytes\n", sb.st_size);
        printf("Hardlink count: %ld\n", sb.st_nlink);
    } else {
        perror("stat");
    }
}
```

## Klonowanie deskryptorów: `dup()` i `dup2()`

Czasem potrzebujemy kilku numerów deskryptorów wskazujących na **ten sam otwarty plik**.  
Do tego służą funkcje:

```c
int newfd = dup(int oldfd);
int newfd = dup2(int oldfd, int desiredfd);
```

- `dup(fd)` – tworzy nowy deskryptor o **najmniejszym wolnym numerze**.  
- `dup2(fd, newfd)` – klonuje deskryptor dokładnie do numeru `newfd` (nadpisuje go, jeśli był otwarty).  

Oba deskryptory współdzielą ten sam **offset** i prawa dostępu.

### Przykład z `dup()`
```c
int fd = open("plik.txt", O_RDONLY);
int fd2 = dup(fd);   // fd2 wskazuje na to samo co fd
```

### Przykład z `dup2()`
Przekierowanie stdout (fd=1) do pliku:
```c
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, 1);        // stdout -> out.txt
printf("Hello!\n"); // zapisze się do pliku, nie na ekran
```

### Zastosowania
- Przekierowania w shellu (`>` i `2>`).
- Pipe’y (`|`) – łączenie procesów.
- Współdzielenie deskryptorów między procesami po `fork()`.

---






