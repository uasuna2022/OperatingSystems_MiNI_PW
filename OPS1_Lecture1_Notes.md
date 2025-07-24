# Systemy Operacyjne 1 – Wykład 1 - Wprowadzenie 

## Spis treści
1. [Definicja systemu operacyjnego](#definicja-systemu-operacyjnego)
2. [Cztery składniki systemu komputerowego](#cztery-składniki-systemu-komputerowego)
3. [Podstawowe tryby przetwarzania](#podstawowe-tryby-przetwarzania)
4. [Zadania systemu operacyjnego](#zadania-systemu-operacyjnego)
5. [Zasoby systemu komputerowego](#zasoby-systemu-komputerowego)
6. [Podsystemy systemu operacyjnego](#podsystemy-systemu-operacyjnego)
7. [Funkcje systemowe i API](#funkcje-systemowe-i-api)
8. [Przerwania i pułapki](#przerwania-i-pułapki)
9. [Ochrona sprzętowa](#ochrona-sprzętowa)

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





