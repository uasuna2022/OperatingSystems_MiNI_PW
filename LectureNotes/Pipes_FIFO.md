# Łącza i FIFO 

**Lącza** (ang. *pipes*) to mechanizm Komunikacji Międzyprocesowej (IPC), który pozwala na przesyłanie danych (strumienia bajtów) między procesami. Działają one na zasadzie *rury*, gdzie jeden proces może pisać dane, a drugi odczytywać je. Działają w trybie *jednokierunkowym* i w trybie FIFO, co oznacza, że dane są odczytywane w kolejności, w jakiej zostały zapisane.

### Dlaczego używamy łączy zamiast wcześniej poznanych metod synchronizacji (semafory, muteksy, CV itd.)?

+ Wcześniej poznane metody synchronizacji (**semafory, muteksy, CV, bariery** itd.) służą raczej do **synchronizacji dostępu do zasobów** (np. do tzw. **sekcji krytycznych**). Poza tym częściej one są stosowane w komunikacji **międzywątkowej** w obrębie **jednego procesu**. Jest to możliwe, ponieważ wątki mogą współdzielić pamięć, czyli wszyscy mogą mieć dostęp do tych samych danych.
Warto oznaczyć, że semafory, muteksy itd. de facto **nie służą do przesyłania danych**. Owszem, można je wykorzystać do synchronizacji dostępu do danych, ale same w sobie nie są mechanizmem przesyłania informacji. 
+ **Łącza i FIFO** są natomiast **specjalnie zaprojektowane do przesyłania danych** między procesami, które mogą być uruchomione na różnych maszynach lub w różnych przestrzeniach adresowych. Dzięki temu umożliwiają komunikację między procesami, które nie mają wspólnej pamięci. Dużą zaletą łączy jest to, że są one **proste w użyciu** i **nie wymagają skomplikowanej synchronizacji**. Procesy mogą po prostu pisać i czytać dane z łącza, a system operacyjny zajmuje się resztą. (Np. nie musimy ręcznie blokować dostępu do łącza, aby uniknąć konfliktów między procesami. System operacyjny automatycznie zarządza dostępem do łącza, zapewniając, że dane są przesyłane w sposób bezpieczny i spójny.)

## Jak wyglądają łącza?

- Łącza posiadają **dwa końce**: jeden do pisania, a drugi do czytania. Dostęp do tych końców jest realizowany za pomocą **deskryptorów plików**. Dane w łączach nie znajdują się na dysku, są przechowywane w pamięci RAM jądra systemu operacyjnego. Procesy mają dostęp do danych w łączach poprzez odwoływanie się do odpowiednich deskryptorów plików.
- Bufor ma **ograniczoną pojemność** (zazwyczaj 64KB), więc jeśli proces piszący próbuje zapisać więcej danych, niż jest dostępne w buforze, **zostanie zablokowany** do czasu, aż proces czytający zwolni miejsce w buforze. Podobnie, jeśli proces czytający próbuje odczytać dane z pustego łącza, zostanie zablokowany do czasu, aż proces piszący zapisze dane do łącza.
- Dane znikają z łącza natychmiast po ich przeczytaniu. Oznacza to, że łącza są **jednokrotnego użytku** - po odczytaniu danych z łącza, dane te są usuwane z bufora i nie można ich ponownie odczytać.

## Łącza anonimowe (pipes) a łącza nazwane (FIFO)

+ **Łącza anonimowe** (*pipes*) udostępniają komunikację między procesami **spokrewnionymi**, czyli między procesami, które mają wspólnego przodka. Inne procesy nie mogą uzyskać dostępu do tego samego łącza. Nie tworzy się dla nich pliku w systemie plików, tworzone są tylko **2 deskryptory plików** (jeden do czytania, drugi do pisania). Istnieją tylko w pamięci RAM systemu operacyjnego. Czas życia łącza anonimowego jest ograniczony do czasu życia procesów, które go używają, albo do czasu życia deskryptorów. Po zakończeniu pracy tych procesów, łącze zostaje automatycznie usunięte.
+ **Łącza nazwane** (*FIFO*) umożliwiają ponadto komunikację między procesami **nie spokrewnionymi**, czyli między dowolnymi procesami. Tworzą one specjalny plik w systemie plików, który może być używany do komunikacji między różnymi procesami. Procesy mogą otwierać ten plik i czytać lub pisać dane do niego, a system operacyjny zajmuje się zarządzaniem dostępem do tego pliku. Czas życia łącza nazwanego jest niezależny od czasu życia procesów, które go używają. Łącze nazwane pozostaje dostępne, dopóki nie zostanie usunięte ręcznie przez użytkownika lub przez system operacyjny.

### Kluczowe różnice pomiędzy łączami a plikami
+ Brak `lseek()`: W łączach nie można używać funkcji `lseek()`, ponieważ łącza są strumieniami danych, a nie plikami z określonymi pozycjami. Oznacza to, że dane są odczytywane i zapisywane w kolejności, w jakiej zostały przesłane, bez możliwości przeskakiwania do określonej pozycji. Próba użycia `lseek()` na łączu zakończy się błędem `ESPIPE`.
+ **Atomowość**: Zapisy małych porcji danych (poniżej stałej `PIPE_BUF` około 4096 bajtów) do łącza są atomowe, co oznacza, że są one przesyłane jako jedna jednostka. Jeśli proces piszący zapisze dane mniejsze niż `PIPE_BUF`, to proces czytający odczyta te dane w całości, bez przerwania. Jądro gwarantuje, że dane od 2 różnych procesów piszących do łącza nie będą wymieszane, jeśli każdy z nich zapisze dane mniejsze niż `PIPE_BUF`. Jeśli jednak proces piszący zapisze dane większe niż `PIPE_BUF`, to jądro może podzielić te dane na mniejsze fragmenty, co może prowadzić do sytuacji, w której proces czytający odczyta tylko część danych, a reszta zostanie odczytana w kolejnych operacjach.
+ **Blokowanie `open()`** (tylko FIFO): Proces otwierający FIFO będzie zawieszony aż do momentu, w którym inny proces otworzy ten sam FIFO w trybie przeciwnym. Na przykład, jeśli proces otworzy FIFO do zapisu, będzie czekał, aż inny proces otworzy ten FIFO do odczytu. Podobnie, jeśli proces otworzy FIFO do odczytu, będzie czekał, aż inny proces otworzy ten FIFO do zapisu. To zachowanie jest specyficzne dla FIFO i nie występuje w przypadku łączy anonimowych.

## Tworzenie łączy
+ **Łącza anonimowe** tworzymy za pomocą funkcji `pipe()`, która przyjmuje jako argument tablicę dwóch elementów typu `int`, do której zostaną zapisane deskryptory plików dla końca do czytania i końca do pisania. Na przykład:

```c
int pipefd[2];
if (pipe(pipefd) == -1) 
{
    perror("pipe");
    exit(EXIT_FAILURE);
}
```
+ **Łącza nazwane** tworzymy za pomocą funkcji `mkfifo()`, która przyjmuje jako argumenty nazwę pliku FIFO oraz tryb dostępu. Na przykład:

```c
if (mkfifo("/tmp/myfifo", 0666) == -1)
{
    perror("mkfifo");
    exit(EXIT_FAILURE);
}
``` 
Teraz jeśli chcemy otworzyć ten FIFO do zapisu, możemy użyć funkcji `open()`:

```c
int fd = open("/tmp/myfifo", O_WRONLY);
if (fd == -1)
{
    perror("open");
    exit(EXIT_FAILURE);
}
```
Aby otworzyć ten FIFO do odczytu, możemy użyć:
```c
int fd = open("/tmp/myfifo", O_RDONLY);
if (fd == -1)
{
    perror("open");
    exit(EXIT_FAILURE);
}
```

## Usuwanie łączy
+ **Łącze anonimowe** (Pipe) zostaje usunięte, kiedy wszystkie procesy zamkną wszystkie deksryptory (zarówno do czytania, jak i do pisania) związane z tym łączem. Dopiero wtedy system operacyjny zwolni zasoby związane z tym łączem.
+ **Łącza nazwane** (FIFO) muszą być usunięte ręcznie, gdy nie są już potrzebne. Można to zrobić za pomocą funkcji `unlink()`, która usuwa plik FIFO z systemu plików. Na przykład: 

```c
if (unlink("/tmp/myfifo") == -1)
{
    perror("unlink");
    exit(EXIT_FAILURE);
}
```
Po wywołaniu `unlink()`, plik FIFO zostanie usunięty, ale jeśli procesy nadal mają otwarte deskryptory do tego FIFO, będą mogły nadal korzystać z niego do momentu, aż zamkną te deskryptory. Dopiero po zamknięciu wszystkich deskryptorów, FIFO zostanie całkowicie usunięte z systemu.


## Mechanizmy zerwania łącza

1. **Pisarz bez czytelnika.**  Gdy w systemie nie ma już **żadnych** otwartych deskryptorów na odczyt dla danego łącza, a prcoes piszący próbuje zapisać dane do tego łącza, to jądro uznaje to za *błąd krytyczny*. System wysyła sygnał `SIGPIPE` do procesu piszącego. Domyślną akcją tego sygnału jest **terminacja** procesu (kończy gwałtownie swoje działanie). Jeśli jednak proces piszący przechwyci ten sygnał i jakoś go zignoruje lub zablokuje, to funkcja `write()` nie zabije programu, ale zwróci -1 i ustawi `errno` na `EPIPE`, co oznacza, że nie można pisać do łącza, ponieważ nie ma czytelnika.

2. **Czytelnik bez pisarza.** Gdy w systemie nie ma już **żadnych** otwartych deskryptorów na zapis dla danego łącza, a proces czytający próbuje odczytać dane z tego łącza, to jądro uznaje to za *koniec pliku* (EOF). Funkcja `read()` zwróci 0, co oznacza, że nie ma więcej danych do odczytania, ponieważ nie ma pisarza. Proces czytający może wtedy zdecydować, co zrobić dalej (np. zakończyć działanie lub spróbować ponownie później).



