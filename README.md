# Projekt SO -- temat 16: Symulacja Dyskontu

### Aby rozpocząć symulację, w katalogu projektu należy wykonać polecenie:
```bash
./start.sh
```
Skrypt start.sh automatycznie kompiluje projekt (przy użyciu Makefile) i uruchamia symulację.

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
* **Stabilna symulacja**: Zrealizowana wszystkie wymagania projekowe. Program stabilnie obsługuje kilkadziesiąt procesów naraz.
* **Logika klientów**: Klienci reagują na sytuację w sklepie. Gdy otwiera się nowa kasa lub inna kolejka idzie szybciej, proces klienta przelicza opłacalność i zmienia kolejkę.
* **Scenariusze obsługi**: Poprawnie zaimplementowano komunikację dwukierunkową między kasą a obsługą. Gdy Klient próbuje kupić alkohol przy kasie samoobsługowej, wzywana jest obsługa aby sprawdzić jego wiek.
* **Osobne okno kierownika**: Wykorzystanie `tmuxa` pozwoliło na podział jednego terminala na parę okien, separując sterowanie Kierownika od logów.
* **Walidacja wejścia:** W procesie Kierownika zaimplementowano sprawdzanie danych wprowadzanych z klawiatury. Program sprawdza, czy wpisana opcja jest liczbą, czy numer wybranej kasy jest poprawny oraz czy użytkownik nie próbuje otworzyć już otwartej kasy.
* **Czytelne logi**: Zastosowałem `AtomicLogger` z semaforem aby bez race conditions wypisywać komunikaty na ekran i do pliku, zgodnię z kolejnością wykonania.
* **Rozróżnianie Logów**: Dodałem kolorowanie wyjścia na ekran, aby łatwiej było rozróżnić procesy.

## Problemy napotkane w projekcie i rozwiązania:
* **Procesy zombie**: Poprzez dużą ilość kończących się procesów klientów, tworzyły się procesy zombie. Rozwiązałem to poprzez obsłużenie sygnału `SIGCHLD` który wywoływał `waitpid` w dyskoncie.
* **Algorytm zmiany kolejki**: Dość czasochłonne było wymyślenie algorytmu decyzji klienta do której kasy powinien podejść. Teraz klient na podstawie ilości klientów i czasu ile by zajełoby mu obsłużenie przy danej kasie, podejmuje najlepsza decyzje, i zmienia ją w zależności od sytuacji przy kasach.

## Pseudokod algorytmu zmiany kolejki:

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
            JEŚLI (kasa_samoobslugowa jest otwara) ORAZ (czas_nowej_kolejki + czas_zmiany < najkrotszy_czas) TO:
                najkrotszy_czas = czas_nowej_kolejki
                najlepszy_wybor = 0

    ZWRÓĆ najlepszy_wybor
KONIEC FUNKCJI
```

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
* `open()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L184]
* `close()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L198]
* `write()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L191]

#### B. Tworzenie procesów: 
* `fork()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L213]
* `execl()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L216]
* `exit()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/kasa.cpp#L314]
* `wait()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L420]

#### C. Obsługa sygnałów:
* `kill()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/kierownik.cpp#L166]
* `signal()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L27]

#### D. Synchronizacja procesów:
* `ftok()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L40]
* `semget()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L46]
* `semctl()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L58]
* `semop()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L74]

#### E. Segmenty pamięci dzielonej:
* `shmget()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp?plain=1#L56]
* `shmat()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L59]
* `shmdt()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/kasa.cpp#L313]
* `shmctl()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L438]

#### F. Kolejki komunikatów:
* `msgget()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L81]
* `msgsnd()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/kasjer.cpp#L197]
* `msgrcv()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/kasa.cpp#L80]
* `msgctl()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/dyskont.cpp#L440]

#### G. Obsługa błedów:
* `perror()`: [https://github.com/Dysky2/dyskont/blob/735b70726e22ca86215c7e4c9c6c99487f4b1d4e/utils.cpp#L18]