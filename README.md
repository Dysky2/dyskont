# Projekt SO -- temat 16: Symulacja Dyskontu

### Wersja i dystrybucje

|      |       |
| :--- | :--- |
| **Operating System:** | Ubuntu 22.04.5 LTS (WSL2) |
| **Kernel:** | Linux 6.6.87.2-microsoft-standard-WSL2 |
| **Architecture:** | x86-64 |
| **g++** | (Ubuntu 11.4.0-1ubuntu1~22.04.2) 11.4.0 |

### Wymagania wstępne

* `tmux`
* `make`
* `g++`

### Aby rozpocząć symulację, w katalogu projektu należy wykonać polecenie:
```bash
chmod u+x start.sh
./start.sh
```
Skrypt start.sh automatycznie kompiluje projekt (przy użyciu Makefile) i uruchamia symulację.

## 1. Założenia projektu:

Celem projektu było stworzenie wieloprocesowej symulacji działania dyskontu, w środowisku systemu Linux.

**Główne założenia:**
* **Architektura wieloprocesowa**: Zgodnie z wymaganiami projekt unika rozwiązań scentralizowanych. Każdy element (Klient, Kasa, Obsługa) jest oparty na `fork()` i `exec()` co oznacza, że jest osobnym procesem.
* **Komunikacja**: Wykorzystano mechanizmy komunikacji Systemu V takie jak kolejki komunikatów czy pamięć dzielona, ustawiając na nich minimalne prawa dostępu.
* **Bezpieczeństwo**: Wszystkie dane wejściowe są walidowane. Błędy systemowe są obsługiwane przez `perror()` i `errno`.
* **Zarządzanie zasobami**: Projekt rozdziela logikę systemową od struktur. Gwarantuje również sprzątanie zasobów przy zamknięciu symulacji.

## 2. Opis ogólny kodu:
#### Pliki Źródłowe:
1.  **`dyskont.cpp`:**
    * Inicjalizuje zasoby systemowe (segmenty SHM, grupy semaforów, kolejki MSQ).
    * Uruchamia procesy potomne kasy, obsługę, kierownika.
    * Generuje procesy klientów w losowym czasie.
    * Zarządza dynamicznie kasami, które trzeba otworzyć a które zamknąć.
    * Odpowiada za wyczyszczenie zasobów po zakończeniu.
2.  **`klient.cpp`:**
    * Generuje listę zakupów (losowe produkty z bazy).
    * Podejmuje decyzję o wyborze kolejki (algorytm `czy_oplaca_sie_zmienic_kolejke`).
    * Wysyła strukturę z zakupami do odpowiedniej kolejki komunikatów (kasy).
    * Obsługuje logikę "niecierpliwości" – sprawdza, czy opłaca się zmienić kolejkę.
3.  **`kasa.cpp`:**
    * Odbiera komunikaty od klientów.
    * Symuluje skanowanie produktów.
    * Wzywa obsługę w razie potrzeby (Awaria wagi, weryfikacja wieku).
    * Generuje paragon dla klienta.
4.  **`kasjer.cpp`:**
    * Działa podobnie do kasy samoobsługowej, ale obsługiwana przez pracownika.
    * Może zostać zamknięta przez Kierownika.
5.  **`obsluga.cpp`:**
    * Odbiera zgłoszenia od kas, symuluje podejście i rozwiązanie problemu.
6.  **`kierownik.cpp`:**
    * Proces umożliwiający użytkownikowi ręczne zarządzanie stanem dyskontu (otwieranie kas, ewakuacja).
7.  **`utils.cpp` i `utils.h`:**
    * Biblioteka funkcji pomocniczych
    * Definicje struktur danych 

## 3. Szczegółowy opis implementacji i algorytmów wraz z pseudokodem

### 3.1 Mechanizmy synchronizacji i IPC
* **Pamięć dzielona:** Przechowuje dane np. w obiekcie `StanDyskontu`, który zawiera tablice PID-ów kas, statusy tych kas oraz długość kolejek.
* **Semafory:** Zestaw semaforów chronią sekcję krytyczną oraz służą do prawidłowej synchronizacji wyjścia na ekran (`SEMAFOR_OUTPUT`).
* **Kolejka komunikatów:** Służą do przekazania informacji klientów do kas oraz do komunikacji między kasami a obsługą (wezwanie pomocy).

### 3.2 Algorytm Klienta
Klient nie wybiera kolejki losowo. Algorytm `czy_oplaca_sie_zmienic_kolejke` podejmuje decyzje w oparciu o aktualną sytuację przy kasach:
1. Klient oblicza szacowany czas potrzebny w obecnej kolejce (pozycja * sredni_czas_obslugi)
2. Sprawdza jakie kasy inne są otwarte
3. Jeżeli `(czas_kolejki_do_ktorej_chce_przejsc + koszt_zmiany) < obecny_czas`, wtedy klient usuwa się z obecnej kolejki i przechodzi do nowej (aktualizując przy tym pamięć dzieloną).

#### Pseudokod algorytmu zmiany kolejki:
```
FUNKCJA czy_oplaca_sie_zmienic_kolejke(pidKlienta, nrKolejki):
    pozycja = znajdz_moja_pozycje_w_kolejce(pidKlienta, nrKolejki)

    JEŚLI pozycja == -1 TO
        ZWRÓĆ -1

    najkrotszy_czas = pozycja * sredni_czas_obslugi[nrKolejki]
    czas_zmiany = 10

    JEŚLI jestem przy kasie samoobsługowej TO
        DLA KAŻDEJ kasy stacjonarnej i (od 1 do 2) WYKONAJ:
            czas_nowej_kolejki = sredni_czas_obslugi[i] * dlugosc_kolejki[i]
            
            JEŚLI (kasa[i] jest otwarta) ORAZ (czas_nowej_kolejki + czas_zmiany < najkrotszy_czas) TO:
                najkrotszy_czas = czas_nowej_kolejki
                najlepszy_wybor = i

    W PRZECIWNYM RAZIE JEŚLI aktualny_nr_kolejki == 1 TO:
        czas_nowej_kolejki = sredni_czas_obslugi[2] * dlugosc_kolejki[2]
        JEŚLI (kasa_nr_7 jest otwarta) ORAZ (czas_nowej_kolejki + czas_zmiany < najkrotszy_czas) TO:
            najkrotszy_czas = czas_nowej_kolejki
            najlepszy_wybor = 2

        JEŚLI przynajmniej jedna kasa samoobsługowa jest otwarta TO:
            czas_nowej_kolejki = sredni_czas_obslugi[0] * dlugosc_kolejki[0]
            JEŚLI (kasa_samoobslugowa jest otwarta) ORAZ (czas_nowej_kolejki + czas_zmiany < najkrotszy_czas) TO:
                najkrotszy_czas = czas_nowej_kolejki
                najlepszy_wybor = 0

    ZWRÓĆ najlepszy_wybor
KONIEC FUNKCJI
```

### 3.3 Obsługa błędów
Zgodnie z wymaganiami, zaimplementowano własne funkcje do obsługi błędów:
*  Funkcja `checkError(int result, const char * msg)` sprawdza wynik wywołania danej funkcji systemowej, pod polem `result`. Jeśli zwróci ona `-1`, wypisuje się wtedy błąd przy pomocy `perror()` z podanym komunikatem, i proces zostaje zakończony.
* Dane wejściowe w `kierownik.cpp` są walidowane, aby zapobiec nieprawidłowemu wykonaniu (Podanie znaków zamiast cyfr).

### 3.4. Synchronizacja wypisywania (AtomicLogger)
Przy wielu procesach równoczesne pisanie na `stdout` powodowało "rozjeżdżanie się" tekstu. Zastosowano klasę AtomicLogger, która synchronizuje dostęp do terminala za pomocą semafora. Mechanizm działania opiera się na **semaforze** (`SEMAFOR_OUTPUT`), który chroni sekcję krytyczną odpowiedzialną za wypisanie danych. 

Każdy proces:
1. Proces przygotowuje komunikat w buforze.
2. Blokuje dostęp do terminala (operacja `P` na semaforze).
3. Wypisuje sformatowany komunikat (z sygnaturą czasową i odpowiednim kolorem) na ekran oraz do dedykowanego pliku logów.
4. Zwalnia blokadę (operacja `V`), umożliwiając dostęp innym procesom.

## 4. Problemy napotkane w projekcie i rozwiązania:
* **Procesy zombie**: Poprzez dużą ilość kończących się procesów klientów, tworzyły się procesy zombie. Rozwiązałem to poprzez obsłużenie sygnału `SIGCHLD` który wywoływał `waitpid` w dyskoncie.
* **Algorytm zmiany kolejki**: Dość czasochłonne było wymyślenie algorytmu decyzji klienta do której kasy powinien podejść. Teraz klient na podstawie liczby osób w kolejece i czasu potrzebnego na obsłużenie przy danej kasie, podejmuje najlepsza decyzje, i zmienia ją w zależności od sytuacji przy kasach.

## 5. Elementy wyróżniające
W projekcie zrealizowano elementy wymienione z punktu 5.3.d:
1. **Integracja z `tmux`:** Skrypt startowy `start.sh` oraz kod `dyskont.cpp` automatycznie dzielą okno terminala, uruchamiając panel Kierownika w osobnym podoknie obok logów symulacji.
2. **Kolorowanie logów:** Zdefiniowano makra ANSI (np. `KOLOR_YELLOW`, `KOLOR_CYAN`) w `utils.h`. Każdy typ procesu (Klient, Kasa, Kierownik) posiada swój unikalny kolor w logach, co poprawia przeglądanie wyników z ekranu.
3. **Zaawansowany "Paragon":** Kasy generują sformatowany, realistycznie wyglądający paragon z losowymi numerami kart i sumą zakupów.

## 6. Testy
* **Test 1 (Obciążenie)**: 
    * Opis: Symulacja wpuszcza naraz 1000 klientów w bardzo krótkim czasie.
    * Stałe ustawione: 
        * [utils.h-45] MAX_ILOSC_KLIENTOW = 1000
        * [utils.h-63] CZAS_KASOWANIA_PRODUKTOW 1
        * [utils.h-64] CZAS_ROBIENIA_ZAKUPOW 1
        * [utils.h-65] CZAS_WPUSZANIA_NOWYCH_KLIENTOW 1
        * [utils.h-66] CZAS_DZIALANIA_OBSLUGI 1
    * Oczekiwany rezultat: System nie ulega awarii, dyskont poprawnie obsługuje każdego klienta, sprawdza czy żaden z limitów nie został przekroczony.
    * Logi: 
        ```
        17:05:43 | [DYSKONT] wchodzi klient 70200
        17:05:43 | [DYSKONT] Jest: 1000 klientow w dyskoncie
        ```
    * Wynik: [**Zaliczony**]
* **Test 2 (Otwarcie kas)**: 
    * Opis: Dyskont poprawnie otwiera wszystkie kasy zgodnie z ilością klientów w sklepie i ilością klientów w kolejce.
    * Oczekiwany rezultat: Po wejściu do sklepu 42 klientów dyskont powinien poprawnie otworzyć 7 kas (6 samoobsługowych i 1 stacjonarną).
    * Logi:
        ```
        17:52:49 | [DYSKONT] otwieram kase stacjonarną 1
        17:52:53 | [DYSKONT] OTWIERAM KASE 4
        17:52:59 | [DYSKONT] OTWIERAM KASE 5
        17:53:02 | [DYSKONT] OTWIERAM KASE 6
        ```
    
    * Wynik: [**Zaliczony**]
* **Test 3 (Weryfikacja wieku)**: 
    * Opis: Klient kupujący alkohol, zostanie sprawdzony przez obsługę, czy ma do tego prawo.
    * Oczekiwany rezultat: Jeżeli klient jest pełnoletni, alkohol zostanie mu sprzedany, jeśli nie, zostanie odstawiony na półkę.
    * Logi:
        ```
        18:16:39 | [KASA-90567] Potrzebna weryfikacja wieku dla: 90755
        18:16:44 | [OBSLUGA] Klient jest pelnoletni, moze kupic alkohol, przy kasie: 90567
        ```
    * Wynik: [**Zaliczony**]
* **Test 4 (Zamknięcie kas)**: 
    * Opis: Kierownik zarządza otwarcie kasy stacjonarnej 2 od razu po otwarciu sie dyskontu.
    * Oczekiwany rezultat: Jeżeli przez 30 sekund nikt nie pojawi sie w kolejce do tej kasy, powinna się ona zamknąć.
    * Stałe ustawione: 
        * [utils.h-64] CZAS_ROBIENIA_ZAKUPOW 50
        * [utils.h-65] CZAS_WPUSZANIA_NOWYCH_KLIENTOW 50
    * Logi:
        ```
        18:27:04 | [Kierownik] Zarzadam otwarcie kasy stacjonarnej 2
        18:27:36 | [KASJER-95210] Idzie na przerwe
        ```
    * Wynik: [**Zaliczony**]
* **Test 5 (Posprzątanie zasobów)**:
    * Opis: Podczas trwania symulacji, zostanie wysłany sygnał SIGINT (CTRL + C).
    * Oczekiwany rezultat: Symulacja powinna poprawnie usunąć wszystkie zasoby, z których korzystała.
    * Logi:
        ```
        18:45:04 | [KLIENT-101099] Wchodzi do sklepu
        18:45:04 | [KLIENT-101100] Wchodzi do sklepu
        ^C
        18:45:05 | [KLIENT-101100] WYCHODZI ZE SKLEPU
        18:45:05 | [KLIENT-101099] WYCHODZI ZE SKLEPU
        18:45:05 | [KLIENT-101065] WYCHODZI ZE SKLEPU
        18:45:05 | [KLIENT-101087] WYCHODZI ZE SKLEPU
        18:45:05 | [KASA-101051] Koniec
        18:45:05 | [OBSLUGA] Koncze prace 

        ipcs -a

        ------ Message Queues --------
        key        msqid      owner      perms      used-bytes   messages

        ------ Shared Memory Segments --------
        key        shmid      owner      perms      bytes      nattch     status

        ------ Semaphore Arrays --------
        key        semid      owner      perms      nsems

        ```
    * Wynik: [**Zaliczony**]
* **Test 6 (Waga towaru)**:
    * Opis: Może zdarzyć się sytuacja w której waga towaru nie zgadza się z produktem który klient dostarczył.
    * Oczekiwany rezultat: Obługa podejdzie do kasy zgłaszającej błąd i odblokuje kasę, aby klient mógł dokończyć zakupy.
    * Logi:
        ```
        18:50:31 | [KASA-102786] Waga towaru sie nie zgadza, prosze poczekac na obsluge 
        18:50:35 | [OBSLUGA] Waga przy kasie 102786 zostala naprawiona
        ```
    * Wynik: [**Zaliczony**]
* **Test 7 (Sygnał ewakuacji)**:
    * Opis: Kierownik zarządza ewakuację całego dyskontu.
    * Oczekiwany rezultat: Klienci opuszczają dyskont bez robienia zakupów, następnie zamykane są wszystkie kasy.
    * Logi:
        ```
        18:53:26 | [Kierownik] Kierownik zarzadzil zamkniecie sklepu
        18:53:26 | [KLIENT-103802] WYCHODZI ZE SKLEPU
        18:53:26 | [KLIENT-103814] WYCHODZI ZE SKLEPU
        18:53:26 | [KLIENT-103820] WYCHODZI ZE SKLEPU
        18:53:27 | [DYSKONT] Zarzadam wyjscie kierownika
        18:53:27 | [DYSKONT] Zarzadam wyjscie obslugi
        18:53:27 | [OBSLUGA] Koncze prace 
        18:53:27 | [DYSKONT] WYLACZENIE KAS
        18:53:27 | [KASA-103755] Koniec
        18:53:27 | [KASA-103756] Koniec
        18:53:27 | [KASA-103759] Koniec
        18:53:27 | [KASJER-103762] Zamykam kase stacjonarna
        ```
    * Wynik: [**Zaliczony**]
* **Test 8 (Zmiana kasy)**:
    * Opis: Klient dynamicznie zmienia kolejkę na najkorzystniejszą w reakcji na tłok lub otwarcie nowej kasy.
    * Oczekiwany rezultat: Klient poprawnie przepina się do nowej kolejki, aktualizując stan w pamięci dzielonej.
    * Logi:
        ```
        18:51:24 | [KLIENT-333922] Zmienia kase z 0 do 1
        18:51:24 | [KLIENT] Ilosc ludzi w kolejce 13 1 0 
        18:51:24 | [KLIENT-334035] Zmienia kase z 0 do 1
        18:51:36 | [Kierownik] Zarzadam otwarcie kasy stacjonarnej 2
        18:51:33 | [KLIENT-334140] Zmienia kase z 0 do 1
        18:51:33 | [KLIENT-334334] Staje do kolejki 0
        18:51:34 | [KLIENT-334301] Staje do kolejki 0
        18:51:35 | [KLIENT-334114] Zmienia kase z 0 do 1
        18:51:36 | [KLIENT-334035] Ide do kasy nr: 1
        18:51:36 | [KLIENT-334034] Zmienia kase z 0 do 2
        18:51:36 | [KLIENT-334113] Zmienia kase z 0 do 2
        ```
    * Wynik: [**Zaliczony**]

## 7. Wymagane funkcje systemowe i linki do kodu

#### A. Tworzenie i obsługa plików:
* `open()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L197]
* `close()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L211]
* `write()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L220]

#### B. Tworzenie procesów: 
* `fork()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L216]
* `execl()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L219]
* `exit()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/kasa.cpp#L327]
* `wait()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L499]

#### C. Obsługa sygnałów:
* `kill()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/kierownik.cpp#L185]
* `signal()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L36]

#### D. Synchronizacja procesów:
* `ftok()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L40]
* `semget()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L46]
* `semctl()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L58]
* `semop()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L87]

#### E. Segmenty pamięci dzielonej:
* `shmget()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L55]
* `shmat()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L57]
* `shmdt()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/kasa.cpp#L326]
* `shmctl()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L511]

#### F. Kolejki komunikatów:
* `msgget()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L82]
* `msgsnd()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/klient.cpp#L201]
* `msgrcv()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/kasa.cpp#L85]
* `msgctl()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/dyskont.cpp#L513]

#### G. Obsługa błędów:
* `perror()`: [https://github.com/Dysky2/dyskont/blob/c4080519410816b44b5ea958ce372b29f04b832f/utils.cpp#L18]