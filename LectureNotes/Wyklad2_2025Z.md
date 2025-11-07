# Wykład 2 - 15-10-2025
elixir.bootlin.com - fajna strona do linuxa

```c
int main (int argc, const char** argv)
{
    printf("Hello, World!\n"); // ta funkcja wywołuje syscalle tego programu (dużo syscalli, do których użytkownik OS nie ma żadnego dostępu)
    return 0;
}

```

```bash
strace ./helloC > output.txt  # przechwytuje i wyświetla wszystkie syscalle 
```

Zawsze trzeba sprawdzać, co system powiedział. Każdy syscall coś zwraca i prawie każdy może nie powieść. 

### mmap()
- Pozwala traktować plik jako powiedzmy tablicę (np. mmap[1000] - tysięczny znak pliku)
- Mapuje plik do przestrzeni adresowej pamięci.
- System łączy fragment pliku na dysku z fragmentem pamięci RAM, a więc dane są wczytywane "leniwie" (dopiero wstedy, gdy są dotykane).
- Główna zaleta - zamiast kopiować cały plik do pamięci RAM i mieć "pośrednika" (jadro) odczytujemy te bajty, które są potrzebne dokładnie w ten moment. 
- Warto stosować dla dużych plików i podczas współdzielenia pamięci pomiędzy procesami.

### Różnica pomiędzy `write` a `printf`
Bardzo ważne jest **minimalizować liczbę syscalli**, bo syscalle są bardzo wolne. 
`printf` - funkcja standardowej biblioteki C
`write` - syscall Linuxa

### Do sprawdzenia
Payload?
