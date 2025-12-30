# Projekt SO -- temat 16: Symulacja Dyskontu

### Aby rozpocząć symulację, w katalogu projektu należy wykonać polecenie:
```bash
./start.sh
```

## Założenia projektu:
* **Architektura wieloprocesowa**: Zgodnie z wymaganiami projekt unika rozwiązań scentralizowanych, jest on oparty na `fork()` i `exec()`
* **Komunikacja**: Wykorzystano mechanizmy komunikacji Systemu V takie jak kolejki komunikatów i pamięć dzielona, ustawiając na nich minimalne prawa dostępu.
* **Bezpieczeństwo**: Wszystkie dane wejściowe są walidowane. Błędy systemowe są obsługiwane przez `perror()` i `errno`
* **Zarządzanie zasobami**: Projekt rozdziela logikę systemową od struktur. Gwarantuje również sprzątanie zasobów przy zamknięciu symulacji.

## Opis ogólny kodu:
* **Proces główny (Dyskont)**: Inicjalizuje zasoby systemowe (segmenty SHM, grupy semaforów, kolejki MSQ), odpowiada za generowanie procesów klientów oraz końcowe sprzątanie zasobów.
* **Proces Klienta**: Symuluje zachowanie konsumenta: wybiera produkty, oblicza potencjalny czas oczekiwania i podejmuje decyzję o zmianie kolejki do kasy o mniejszym obciążeniu.
* **Proces Kasy**: Modeluje kasę samoobsługową. Pobiera dane o produktach przez kolejki MSQ i komunikuje się z procesem obsługi w sytuacjach spornych (weryfikacja wieku, błędy wagi).
* **Proces Kasjera**: Proces obsługujący kasy stacjonarne, reagujący na polecenia Kierownika oraz sygnały systemowe.
* **Proces Obsługi**: Proces pomocniczy, który na wezwanie kasy symuluje podejście do stanowiska i rozwiązanie problemu (np. zatwierdzenie wieku klienta).
* **Proces Kierownika**: Osobny interfejs sterujący (uruchamiany w tmux), umożliwiający użytkownikowi ręczne zarządzanie stanem dyskontu (otwieranie kas, ewakuacja).

## Co udało się zrobić:
*   **Stabilna symulacja**: Udało się zrealizować wszystkie punkty z wymagań. Program stabilnie obsługuje kilkadziesiąt procesów naraz.
*   **Logika klientów**: Klienci nie są tylko tłem – faktycznie reagują na sytuację w sklepie. Gdy otwiera się nowa kasa lub inna kolejka idzie szybciej, proces klienta przelicza opłacalność i zmienia kolejkę.
*   **Scenariusze obsługi**: Poprawnie zaimplementowano komunikację dwukierunkową między kasą a obsługą. Gdy Klient próbuje kupić alkohol przy kasie samoobsługowej, wzywana jest obsługa aby sprawdzić jego wiek.
*   **Osobne okno kierownika**: Poprzez wykorzystanie `tmuxa` można zarządzać panelem kierownika w jednym terminalu.
*   **Czytelne logi**: Zastosowałem `AtomicLogger` z semaforem aby bez race conditions wypisywać komunikaty na ekran i do pliku, zgodne z kolejnościa wykonania.

## Testy
* **Test 1 (Obciążenie)**: 
    * Opis: Symulacja wpuszcza naraz 100 klientów w bardzo krótkim czasie.
    * Oczekiwany rezultat: System nie ulega awarii, dyskont poprawnie obsługuje każdego klienta, sprawdza czy żaden z limitów nie został przekroczony.
    * Wynik: [**Zaliczony**]
* **Test 2 (Otwarcie kas)**: 
    * Opis: Dyskont poprawnie otwiera wszystkie kasy zgodnie z ilością klientów w sklepie
    * Oczekiwany rezultat: Po wejściu do sklepu 25 klientów dyskont powinien poprawnie otworzyć 7 kas (6 samoobsługowych i 1 stacjonarną)
    * Wynik: [**Zaliczony**] 
* **Test 3 (Weryfikacja wieku)**: 
    * Opis: Klient kupujący alkohol, zostanie sprawdzony przez obsługę, czy ma do tego prawo
    * Oczekiwany rezultat: Jeżeli klient jest pełnoletni, alkohol zostanie mu sprzedany, jeśli nie, zostanie odstawiony na półkę.
    * Wynik: [**Zaliczony**] 
* **Test 4 (Zamkniecie kas)**: 
    * Opis: Kierownik otwiera kase stacjonarna 2 od razu po otwarciu sie dyskontu
    * Oczekiwany rezultat: Jeżeli przez 30 sekund nikt nie pojawi sie w kolejce do tej kasy, powinna się ona zamknąć
    * Wynik: [**Zaliczony**] 
* **Test 5 (Posprzatanie zasobów)**:
    * Opis: Podczas trwania symulacji, zostanie wysłany sygnał SIGINT (CTRL + C)
    * Oczekiwany rezultat: Symulacja powinna poprawnie usunac wszystkie zasoby, z których korzystała
    * Wynik: [**Zaliczony**] 
* **Test 6 (Waga towaru)**:
    * Opis: Może zdarzyć się sytuacja w której waga towaru nie zgadza się z produktem który klient dostarczył
    * Oczekiwany rezultat: Obługa podejdzie do kasy zgłaszającej błąd i odblokuję kasę, aby klient mógł dokończyć zakupy
    * Wynik: [**Zaliczony**] 
* **Test 7 (Sygnał ewakuacji)**:
    * Opis: Kierownik zarządza ewakuację całego dyskontu 
    * Oczekiwany rezultat: Klienci opuszczają dyskont bez robienia zakupów, następnie zamykane są wszystkie kasy
    * Wynik: [**Zaliczony**]

## Wymagana funkcje systemowe i linki do kodu

#### A. Tworzenie i obsługa plików:
* `open()`: []
* `close()`: []
* `write()`: []

#### B. Tworzenie procesów: 
* `fork()`: []
* `execl()`: []
* `exit()`: []
* `wait()`: []

#### C. Obsluga sygnałów:
* `kill()`: []
* `signal()`: []

#### D. Synchronizacja procesów:
* `ftok()`: []
* `semget()`: []
* `semctl()`: []
* `semop()`: []

#### E. Segmenty pamięci dzielonej:
* `shmget()`: []
* `shmat()`: []
* `shmdt()`: []
* `shmctl()`: []

#### F. Kolejki komunikatów:
* `msgget()`: []
* `msgsnd()`: []
* `msgrcv()`: []
* `msgctl()`: []

#### G. Obsługa błedów:
* `perror()`: []