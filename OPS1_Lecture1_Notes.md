# Systemy Operacyjne 1 – Wykład 1 - Wprowadzenie 

## Spis treści
1. [Definicja systemu operacyjnego](#definicja-systemu-operacyjnego)
2. [Cztery składniki systemu komputerowego](#cztery-składniki-systemu-komputerowego)
3. [Podstawowe tryby przetwarzania](#podstawowe-tryby-przetwarzania)
4. [Zadania systemu operacyjnego](#zadania-systemu-operacyjnego)
5. [Zasoby systemu komputerowego](#zasoby-systemu-komputerowego)
6. [Cykl instrukcji CPU](#cykl-instrukcji-cpu-cpu-instruction-cycle)

---

## Definicja systemu operacyjnego  <a id="definicja-systemu-operacyjnego"></a>

> **Nie istnieje jednej ściśle przyjętej definicji systemu operacyjnego.**  
> Różne opisy podkreślają różne role:

| Rola | Opis |
|--------|--------------------------|
| **Pośrednik między użytkownikiem a sprzętem** | OS tłumaczy polecenia użytkownika na sygnały dla CPU, dysku, ekranu itp. |
| **Alokator zasobów** | Przydziela procesom czas **CPU** (Central Processing Unit), **RAM** (Random Access Memory), **pasmo I/O** (ilość danych na sekundę, które mogą byś przesłane między procesorem/pamięcią a urządzeniami zewnętrznymi) tak, by nikt nie blokował reszty. |
| **Program sterujący innymi programami** | Nadzoruje uruchamianie, wstrzymywanie i kończenie procesów, pilnuje ich izolacji. De facto mówi procesorowi, jakie instrukcje wykonywać i kiedy. |
| **Podstawowe oprogramowanie systemu** | Minimalny zestaw kodu i narzędzi, który pozwala komputerowi funkcjonować i udostępnia usługi programom użytkownika. Zawiera jądro, sterowniki, biblioteki oraz zestaw bazowych narzędzi. Bez tej wartswy sprzęt jest praktycznie nieużyteczny. |
| **Jądro (kernel)** | Centralna, uprzywilejowana część systemu operacyjnego. Ma bezpośredni dostęp do CPU, RAM, dysków, portów itd. Zarządza procesami, pamięcią, plikami, przerwaniami, zapewnia ochronę, obsługuje wywołania systemowe. |

## Cztery składniki systemu komputerowego <a id="cztery-składniki-systemu-komputerowego"></a>

Komputerowy system operacyjny można podzielić na cztery główne warstwy (od najniższej do najwyższej):

1. **Sprzęt (hardware)**  
   - CPU, RAM, urządzenia I/O (dysk, karta graficzna, sieć)
   - Udostępnia *surową* moc obliczeniową

2. **System operacyjny (OS)**  
   - Steruje sprzętem, zarządza pamięcią i procesami
   - Udostępnia programom interfejs systemowy (API, wywołania systemowe)

3. **Programy użytkownika (application programs / user space)**  
   - Aplikacje uruchamiane przez użytkownika: terminal, edytor tekstu, przeglądarka itp.
   - Działają w trybie użytkownika i nie mają bezpośredniego dostępu do sprzętu

4. **Użytkownicy (users)**  
   - Osoby pracujące z systemem
   - Uruchamiają programy, wydają polecenia, odbierają wyniki

> Tylko system operacyjny ma bezpośredni kontakt ze sprzętem. Reszta komunikuje się z nim przez warstwy pośrednie.


## Zasoby systemu komputerowego <a id="zasoby-systemu-komputerowego"></a>

System operacyjny zarządza wszystkimi zasobami sprzętowymi komputera – przede wszystkim:

### 🧠 Procesory (CPU)
- OS decyduje, które zadania będą uruchamiane na którym rdzeniu.
- Różne rdzenie mogą wykonywać:
  - zadania użytkownika (np. `firefox`, `make`)
  - usługi systemowe (np. `sshd`, `udevd`)
  - nic (tryb „idle” – oczekiwanie na zadanie)

### 🧠 Pamięć operacyjna (RAM)
- RAM jest podzielona na obszary:
  - **obszar jądra** – zarezerwowany, niedostępny dla użytkownika
  - **obszary procesów użytkownika** – każdy proces dostaje swój własny kawałek
  - **wolna pamięć** – gotowa do przydzielenia nowym programom
- OS chroni pamięć między procesami (ochrona przed nieautoryzowanym dostępem).

### 💾 Dyski twarde (SSD / HDD)
- OS zarządza zapisem i odczytem danych z dysków.
- Udostępnia abstrakcję plików i folderów (system plików: `ext4`, `NTFS`, `FAT`…).
- Buforuje operacje dyskowe, kontroluje dostęp, chroni dane.

### 📡 Urządzenia I/O (wejścia/wyjścia)
- OS pośredniczy w dostępie do:
  - klawiatury, myszy, monitora, głośników,
  - drukarki, kamer, mikrofonu itp.
- Obsługuje sterowniki, kolejkowanie żądań, synchronizację.

### 🌐 Karta sieciowa (NIC)
- Pozwala na połączenia z innymi komputerami i internetem.
- OS zarządza buforami pakietów, protokołami (np. TCP/IP), dostępem do interfejsów (`eth0`, `wlan0`, `lo`...).

### 🧠 Procesor graficzny (GPU)
- Wykorzystywany w grafice 2D/3D, grach, obliczeniach równoległych.
- Dostępny dla użytkownika poprzez specjalne biblioteki i sterowniki (np. OpenGL, Vulkan, CUDA).

### ⏲️ Timer systemowy
- Generuje przerwania sprzętowe co kilka milisekund.
- OS wykorzystuje go do preempcji (OS przerywa działanie jednego procesu, zeby dać czas procesora innemu procesowi), zarządzania czasem procesów i odliczania timeoutów.

### 🔄 Kontrolery DMA (Direct Memory Access)
- Pozwalają urządzeniom kopiować dane do RAM bez obciążania CPU.
- OS inicjuje operacje DMA i dba o ich bezpieczeństwo.

### 🔌 Nośniki wymienne
- OS obsługuje montowanie i odłączanie pendrive’ów, kart SD, DVD itd.
- Dba o systemy plików, bezpieczeństwo i synchronizację danych.

> Dzięki zarządzaniu tymi wszystkimi zasobami, system operacyjny może zapewniać wielozadaniowość, bezpieczeństwo i efektywność działania.


## Wyzwania zarządzania zadaniami <a id="zadania-systemu-operacyjnego"></a>

### 🔹 Jedno zadanie to marnowanie zasobów
- CPU i RAM są niedostatecznie wykorzystywane, gdy działa tylko jedno zadanie.
- Program może czekać na:
  - dane z sieci,
  - wejście od użytkownika,
  - odpowiedź z dysku.
- Reszta systemu stoi i czeka.

### 🔹 OS musi robić tzw. job scheduling - planowanie zadań
- W systemie może być więcej zadań niż rdzeni.
- OS musi decydować:
  - które zadanie uruchomić,
  - kiedy je wstrzymać i wznowić,
  - jak podzielić czas procesora.

### 🔹 Ograniczona pamięć RAM
- RAM może się wyczerpać.
- OS musi:
  - zarządzać przydziałem pamięci,
  - zwalniać nieużywane obszary.

### 🔹 Równoczesny dostęp do urządzeń I/O
- Kilka procesów może chcieć korzystać z dysku lub karty sieciowej.
- OS musi:
  - synchronizować operacje,
  - kolejkować dostęp,
  - chronić przed konfliktem danych.

## Podstawowe tryby przetwarzania <a id="podstawowe-tryby-przetwarzania"></a>

Użytkownik może zlecić różne zadania, jednak to OS decyduje w jaki sposób je wykonać (w jakiej kolejności, czy pozwalać na bieżące modyfikacje ze strony użytkownika itd.)

Istnieją 3 główne tryby przetwarzania:
- **off-line** (batch) - przetwarzanie wsadowe
- **on-line** (interactive)
- **real time** (czas rzeczywisty)

Tryb pracy determinuje: 
- czy użytkownik może reagować w trakcie działania programu,
- jak ważne jest dokładne trzymanie się limitów czasowych,
- jak system alokuje zasoby i przełącza zadania.

### 1. Tryb wsadowy (*batch*, *off-line*)
W trybie wsadowym użytkownik nie wykonuje programu ręcznie ani nie wchodzi z nim w interakcję.  
Zamiast tego **zadanie jest wrzucane do kolejki zadań (job queue)**, a system operacyjny **sam decyduje, kiedy i jak je wykonać**. Kolejka ta może być priorytetowa.

**Cechy charakterystyczne:**
- **brak interakcji z użytkownikiem** podczas działania zadania,
- nieprzewidywalny czas rozpoczęcia (zależy od dostępnych zasobów i kolejki, jeśli w kolejce znajduje się zbyt dużo zadań i/lub prioprytet wrzuconego zadania jest niski, to możliwe jest oczekiwanie na odpowiedź w ciągu kilku minut/godzin),
- pełne wykorzystanie zasobów – zadanie może zająć cały CPU i RAM,
- zadanie kończy się samo i **zwraca gotowy wynik** (np. log, raport, plik wyjściowy), nie można otrzymać albo podejrzeć wynik dotychczasowy.

**Zalety:**
- proste planowanie,
- wysoka efektywność (można optymalnie przydzielać zasoby).

**Wady:**
- brak elastyczności i reaktywności,
- nie nadaje się do aplikacji interaktywnych ani czasu rzeczywistego.

**Przykład:**   załóżmy, że co noc o 2:00 system automatycznie robi kopię zapasową plików:
```bash
tar -czf backup_$(date +%F).tar.gz /home/maksim
```
Nie siedzimy przy komputerze, nie obserwujemy działania, nie możemy wpłynąć na wykonanie zadania w jego ciągu, tylko rano widzimy, że powstał plik z kopią - czyli widzimy wynik gotowy.

### 2. Tryb interaktywny (*on-line*, *time-sharing*)

W trybie interaktywnym wiele zadań użytkownika działa jednocześnie. System operacyjny dzieli czas procesora na małe kawałki (tzw. *time slice*, np. 20 ms) i **cyklicznie przełącza zadania**.

**Cechy charakterystyczne:**
- użytkownik **może wchodzić w interakcję z systemem w czasie rzeczywistym**,
- OS stosuje **preempcję** – może przerwać jedno zadanie i dać czas CPU innemu,
- każdy proces dostaje „kwant czasu” do wykorzystania,
- dochodzi do **przełączeń kontekstu (context switch)** między zadaniami - OS wstrzymuje zadanie A, zapisuje jego stan (kontekst), ładuje zadanie B w tym stanie, w którym zostało ono wstrzymane i wykonuje zadanie B do kolejnego przełączenia kontekstu,
- działa w tzw. trybie wielozadaniowym (*multitasking*).

**Zalety:**
- płynna praca wielu aplikacji równocześnie,
- wygodność dla użytkownika - wydaje się, że system wykonuje wszystkie zadania równolegle.

**Wady:**
- złożoność planowania,
- potencjalne koszty przełączeń kontekstu,
- mniej efektywne niż wsad dla zadań długich i wymagających dużej ilości zasobów.

**Przykład:**
mamy otwarte trzy aplikacje:
- YouTube w przeglądarce (bieżące wideo),
- edytor tekstu (`notepad.exe`),
- terminal (`bash`) wykonujący `ping`.

System operacyjny **przełącza je co kilkadziesiąt milisekund**, dając wrażenie, że wszystkie działają jednocześnie.  
Kiedy przeglądarka czeka na dane z sieci – CPU przeskakuje do terminala.  
Kiedy wpisujesz coś w edytorze – jego proces dostaje kolejne ułamki czasu.

> Typowe zastosowania: systemy biurkowe (Linux, Windows, macOS), serwery z wieloma użytkownikami (SSH), terminale zdalne.

### 3. Tryb czasu rzeczywistego (*real-time*)

W trybie czasu rzeczywistego system operacyjny **musi zareagować na zdarzenie zewnętrzne w określonym czasie**.  
Ten czas to tzw. **górna granica czasu reakcji** (*reaction time bound*), której nie wolno przekroczyć.

**Cechy charakterystyczne:**
- OS reaguje na zdarzenia w czasie **deterministycznym** (np. do 5 ms),
- zadania mają **priorytety czasowe** i często działają w środowisku z gwarancją zasobów,
- brak możliwości czekania w kolejce — reakcja musi być natychmiastowa,
- tryb używany w systemach **krytycznych**, gdzie opóźnienie może mieć straszne skutki (np. dziedzina transportu).

Rodzaje systemów RT:
| Typ | Opis |
|-----|------|
| **Hard RT** | reakcja **zawsze** musi nastąpić przed upływem limitu. Opóźnienie = awaria. |
| **Soft RT** | opóźnienie jest tolerowane, ale wpływa na jakość działania (np. dźwięk, animacja). |

---

**Przykład:**
wyobraźmy sobie, że drzwi do sejfu zostały otwarte nieautoryzowanie.  
System RT ma **dokładnie 20 ms**, aby uruchomić alarm (czyli wysłać sygnał I/O).  
Jeśli spóźni się choćby o 1 ms, alarm nie zostanie uznany za ważny. Albo np. system ma dokładnie 100 ms, aby silnik samochodu zareagował na sygnał. 

Tego typu system:
- ma zarezerwowane zasoby tylko dla siebie,
- działa w sposób przewidywalny,
- jest stosowany w przemyśle, transporcie, medycynie, lotnictwie.

> Typowe zastosowania: systemy ABS w samochodzie, robotyka przemysłowa, sterowanie sygnalizacją świetlną, monitorowanie pacjenta w szpitalu.

## Cykl instrukcji CPU (CPU instruction cycle) <a id="cykl-instrukcji-cpu-cpu-instruction-cycle"></a>

Procesor wykonuje instrukcje w prostym cyklu:

1. **Fetch** – pobierz instrukcję z pamięci (na podstawie IP – *Instruction Pointer*).
2. **Decode** – zdekoduj ją (rozpoznaj, co oznaczają bajty).
3. **Execute** – wykonaj ją (zmień rejestry, zapisz wynik, wykonaj operację).

### 🔎 Przykład (x86):
```assembly
b8 04 00 00 00   ; instrukcja: MOV EAX, 4
```

- `b8` oznacza: załaduj wartość do rejestru `EAX`
- `04 00 00 00` to liczba `4` w postaci 4 bajtów

CPU:
- pobiera instrukcję z RAM (`fetch`)
- rozpoznaje bajty (`decode`)
- wykonuje (wpisuje 4 do `EAX`) (`execute`)
- zwiększa `IP`, by przejść do kolejnej instrukcji

> Cykl `fetch → decode → execute` jest zapisany fizycznie w sprzęcie CPU. OS i programy tylko dostarczają instrukcje – CPU działa samodzielnie.











