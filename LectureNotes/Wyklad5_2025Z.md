# Wykład 5 - 05-11-2025

`getpid()` - najbardziej trwialny syscall do otrzymania process id

Zwykle procesy tworzą hierarchią. Najczęściej to jest rodzic-dziecko. Jednak może być drzewiasta. Każdy PCB ma wskaźnik na "rodzica" (`parent`). Ma listę wskaźników na procesy-dzieci (`children`). Czasami dziecko dziecka może zostać "zreparentowane" do góry drzewa, jeżeli go rodzic "umarł".

O co proces może poprosić OS?
- stworzyć proces
- zamknąć proces
- komunikować z innymi procesami
- zamknąć siebie
- pushować coś na kolejkę

## Process creation

`fork()` - syscall do robienia procesa-dziecka. Dziecko dziedziczy wiele atrybutów od rodzica. 
Proces musi być w stanie "executing", żeby stworzyć proces-dziecko. Po tworzeniu nowego procesu ten proces żyje, jednak nic nie robi (`ready`). Dziecko NIE natychmiast zaczyna wykonywać. Scheduler podejmuje decyzję. Nie wiemy, kiedy zostanie wykonany który proces. Są wykonywane współcześnie. 

Kod jest dziedziczony od rodzica. Cała przestrzeń adresowa jest dziedziczona od rodzica! Exact copy. Procesy mają jednak izolowaną niezależną pamięć. Kopia, nie jest ten sam obiekt! Proces potomny z punktu widzenia programisty zaczyna z momentu wyjścia z forka. Nie zawołał tego forka, jednak z niej wrócił. Przed forkiem jest jeden proces, po forku są dwa. 

Czy ja jestem dzieckiem, czy ja jestem rodzicem? Na szczycie stosu jest wartość zwrócona z forka. W rodzicu fork zwróci pid dziecka. W dziecku ten fork zwróci 0. To jest jedyna różnica pomiędzy stanem stosów rodzica i dziecka!

`fork()` zwróci coś dwa razy: globalne będą już istnieć dwa stosy z różnymi wartościami `ret`

`getppid()` - process id rodzica

Ten sam kod wykonują dwa różne procesy po fork(). 

Po forku mamy totalnie nowy komplet zmiennych (??)
Nie ma żadnej możliwości dostać się do pamięci innego procesu (chyba że nie jest to pamięć dzielona).

Jak piszemy fork() w pętli z iteratorem 'i', to dzieci wiedzą swoje 'i' i mogą do tego się odnosić. 
Musimy switchować po otworzeniu fork().
```c
int* ptr = (int)malloc(sizeof(int));
int x = 1;

for (int i = 0; i < 5; i++)
{
    int ret = fork();

    switch(ret)
    {
        case -1:
            printf("fork() failed :(\n");
            return 123;
        case 0:
            printf("Hello, world! I'm the %d'th child process! My parent is %d.\n", getpid(), i, getppid());
            *ptr = 5;
            free(ptr);
            x = 2;
            // break; // tak nie można, bo inaczej zostanie stworzone "drzewo procesów", a nie chcemy tego w tym kodzie
            exit(0); // albo tak, 
            return 0; // albo tak
        default:
            printf("Hello, world! I'm the parent process! My parent is %d. My child is %d.\n", 
                getpid(), getppid(), ret);
            x = 3;
            break;
    }
}
```

Potencjalny błąd, który może wystąpić, jeżeli napiszemy `break` zamiast `exit` albo `return`.
R -> D0 -> D1 -> D2
  -> D1 -> D2
  -> D2 

Procesy NIE dziedziczą:
- timer
- wątki
- memory locks
- file locks
- sygnały

Tablica deskryptorów też jest całkiem dziedziczona! Nastąpią współbieżne write'y do tego samego otwartego pliku, jak będziemy chcieli coś zapisać w procesie dziecku. 

## Syscall exit()

Exit(3): 3 - status. PCB sobie nadal żyje, chociaż cała pamięć jest zwalniana. 

## Syscall wait()
PCB dziecka żyje po exit() dopóki w rodzicu nie zostanie wywołany wait(). Potem system całkiem zapomina o tym, że ten proces kiedyś istniał. 
Proces zombie - proces dziecka, który został terminated, jednak rodzic nie wywołał `wait()`, a więc OS nadal posiada info o tym, że ten proces istnieje, jednak on już nie może nic wykonać.

`WIFEXITED` zwróci 1, jeżeli proces zakończył się normalnie.
`WIFSIGNALED` zwróci 1, jeżeli proces się skończył z powodu zabicia sygnałem.

### Orphaned processes (procesy-sieroty)

Jeżeli rodzic umrze nie waitując dziecka, to dziecko zostaje przypięrte do jakiegoś "głównego procesu" systemu, które potem **na pewno** zawoła waita i zabije go. W momencie, kedy rodzic robi `exit()`, sprawdza się, czy ten proces miał jakieś potomstwo, i jeżeli tak, to zostaje przypięte do procesa-"administratora" (?)


```bash
kill 140005 # zabija proces o numerze z basha
```

`wait` zwraca error jak już nie ma dzieci, a więc można obsłużyć to i w taki sposób

Kiedy dziecko się kończy, to system strzela sygnałem w rodzica o tym, że dziecko się skończyło. 

## exec() 
TODO

Nie zawsze argv[0] to ścieżka do pliku.