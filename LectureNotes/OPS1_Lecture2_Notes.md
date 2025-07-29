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

  read(fd, buf, 10);  // czyta bajty [0..9]
  read(fd, buf, 10);  // czyta bajty [10..19]
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