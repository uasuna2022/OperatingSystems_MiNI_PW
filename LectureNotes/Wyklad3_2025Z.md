# Wykład 3 - 22-10-2025

```bash
$$              # oznacza bieżący proces (np. PID = 11969)
ls ./$$/fd      # lista otwartych deskryptorów
ls /dev/pts/4   # plik specjalny (special device)
lsblk           # (list block devices) 
sudo xxd -l 512 /dev/nvme0n1p1  # poprosi o hasło superadmina (nie wiem co to jest)
```

Socket - pliki do nawiązywania połączeń pomiędzy plikami (na SOPach drugich).

W tablicy deskryptorów zawiera się wskaźnik na strukturę, w której zawiera się *mode* (r, w, x itd.), *offset* i *inode*.

Tablica deskryptorów jest per proces (deskryptor 3 w procesie A nie jest tym samym, co deskryptor 3 procesu B).

```c
struct files_struct {/*...*/} // open file table structure
```

errno - globalna zmienna błędu, jest `int`, a więc musimy napisać `stderror(errno)`, żeby zwróciło `char*` i można było odczytać konkretny błąd.

Jak robię `open()` z flagą `O_CREAT`, to nie wiemy, czy otworzył się już istniejący plik, czy został stworzony nowy.

`open()` zawsze odejmuje maskę, która jest atrybutem procesu (umask). Można ją zmienić za pomocą syscalla w kodzie C.

`unlink(char* filepath)` usuwa powiązanie (jak jest jedno, to usuwa plik).

``` c
if (exists)
    fail()
else open()    // tak NIE działa O_EXCL, bo tutaj może być race i potencjalnie obydwa procesy mogą zgarnąć dostęp do pliku. 
               // jest tam zaimplementowany mutex, który ze strony jądra gwarantuje, że tylko jeden proces może mieć otwarty ten plik.
```

### Limit deskryptorów
Istnieje limit otwartych deskryptorów: i = 524285, a więc nieużywane deskryptory trzeba zamykać.
```bash
ls -la /proc/[PID]/fd  # lista wszystkich otwartych deskryptorów
```
Jak wywalić proces, to tablica deskryptorów się zwalnia.

`ssize_t` - typ, który różni się od size_t tylko tym, że może przyjmować wartości ujemne.

Jedyny sposób się upewnić, że plik się skończył - `read()` zwrócił 0. Bo jak np. zamiast 4 bajtów zwrócił 3, to to nie znaczy, że plik się skończył na pewno, może być, że pozostałe bajty system nie może dostarczyć dokładnie w ten moment.

`read()` jest domyślnie blokujący (jak i `write()`), dlatego jak np. będziemy czytać z *stdin*, to program będzie wisieć, oczekiwając na nowe dane. Skończyć to inteligentnie możemy za pomocą *Ctrl + C*. 







## TODO
- poczytać sobie o umask i jak go używać w C

