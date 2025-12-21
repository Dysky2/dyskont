#pragma once

#include <cstdio> 
#include <cstdlib>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

// -- DEFINICJE MAKSYMALNYCH WARTOSCI --

#define MAX_DATA_SIZE 200
#define MAX_ILOSC_KLIENTOW 100

// -- DEFINICJE DLA STRUKTURY DYSKONTU --

#define ILOSC_KOLJEJEK 3
#define MAX_DLUGOSC_KOLEJEK 100
#define ILOSC_KAS 8
#define CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA 10
#define CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA 15

#define ID_KASY_STACJONARNEJ_1 6
#define ID_KASY_STACJONARNEJ_2 7

// -- SEMAFORY --

#define SEMAFOR_SAMOOBSLUGA 0
#define SEMAFOR_STAC1 1
#define SEMAFOR_STAC2 2
#define SEMAFOR_ILOSC_KLIENTOW 3
#define SEMAFOR_ILOSC_KAS 4
#define SEMAFOR_OBSLUGA 5
#define SEMAFOR_LISTA_KLIENTOW 6
#define SEMAFOR_OUTPUT 7

// -- ZMIENNE GLOBALNE -- 

extern const double simulation_speed;
extern const int simulation_time;
extern const int startowa_ilosc_kas;
extern const std::vector<std::vector<int>> products_price;
extern const std::vector<std::vector<std::string>> products;

// -- Funkcje globalne --

// Sprawdzenie bledow wykonania danej funkcjo
int checkError(int result, const char * msg);

// pobieram aktulany czas i przypisuje go to struktury podanej jako argument
void pobierz_aktualny_czas(struct tm *aktualny_czas);

// Tworzy klucz na podstawie znaku
key_t utworz_klucz(const char znak);

// Tworze grupe semaforowa składająca sie z  semaforów
// 0 -> semafor do koleki samooboslugowej
// 1 -> semafor do koleki stacjonarnej 1
// 2 -> semafor do koleki stacjonarnej 2
// 3 -> semafor liczaczy ilosc klientow w sklepie 
// 4 -> semafor odpowiadajacy ile kas aktualnie jest otwartych
// 5 -> semfaor czy obsluga udziela komus pomocy
// 6 -> semfor dla listy klientow ktorzy sa w sklepie ale nie w kolejce
// 7 -> semfor odpowiedzialny za poprawna kolejnosc wypisania komunikatow
int utworz_grupe_semaforowa();

// ustawien wartosci poczatkowe dla wszystkich semaforów
void ustaw_poczatkowe_wartosci_semaforow(int semid);

// Opuszczenie danego semafora o 1
void operacja_p(int semId, int semNum);

// Podniesienie danego semafora o 1
void operacja_v(int semId, int semNum);

std::string utworz_katalog_na_logi();

void ustaw_nazwe_katalogu(std::string nazwa);

// -- STRUKTURY --

struct StanDyskontu {
    int pid_kasy[ILOSC_KAS];
    int status_kasy[ILOSC_KAS]; // 0 -> kasa zamknieta 1 -> kasa otwarta 2-> kasa w stanie opruznniania
    int kolejka_klientow[ILOSC_KOLJEJEK][MAX_DLUGOSC_KOLEJEK];
    int dlugosc_kolejki[ILOSC_KOLJEJEK];

    float sredni_czas_obslugi[ILOSC_KOLJEJEK]; // 0 czas kasy samooblugowe, 1 -> czas kasy stacjonranerej_1, 2 -> czas kasy stacjonranerej_2

    pid_t pid_kierownika;
    pid_t pid_dyskontu;
};

struct Obsluga {
    long int mtype;
    int powod; // 1 -> Sprawdzenie wieku, 2 -> popsuta kasa
    int kasa_id;
    int wiek_klienta;
    int pelnoletni; // 0 -> osobo niepelnetnia 1 -> pelnoletni
};

struct Klient {
    long int mtype;
    int klient_id;
    int nrKasy;
    int wiek;
    int ilosc_produktow;
    char lista_produktow[MAX_DATA_SIZE];
};

struct DaneListyKlientow {
    int ilosc;
    int lista_klientow[MAX_ILOSC_KLIENTOW];
};

struct AtomicLogger {
    std::stringstream bufor;
    struct tm aktualny_czas;
    char bufor_czas[64];

    AtomicLogger();
    ~AtomicLogger();

    template<typename T>
    AtomicLogger& operator<<(const T& wartosc) {
        bufor << wartosc;
        return *this;
    }
};

#define komunikat AtomicLogger()

// -- KLASY --

class ListaKlientow  {
private:
    DaneListyKlientow * klienci;

public:
    ListaKlientow(DaneListyKlientow * klienci_pamiec, int pierwsze_wywolanie);

    int dodaj_klienta_do_listy(int pid_klienta);
    void usun_klienta_z_listy(int pid_klienta);
    void wyswietl_cala_liste();
};

class Kolejka {
private: 
    StanDyskontu * stan_dyskontu;

public:
    Kolejka(StanDyskontu * stan_dyskontu);

    // dodaj na koniec koleki klienta
    void dodaj_do_kolejki(int pid_klienta, int nr_kolejki);

    // usun pierwszego klienta z kolejki
    void usun_z_kolejki(int pid_klienta, int nr_kolejki);

    // wyswietl cala kolejkie pidow
    void wyswietl_kolejke(int nr_kolejki);

    // sprawdz czy podany pid jest pierwszy w kolejce do danej kasy
    int czy_jestem_pierwszy(int pid_klienta, int nr_kolejki) ;

    // szuka w podanym numerze kolejki swojego id i zwraca pozycje na której się znajduje
    int znajdz_moja_pozycje_w_kolejce(int pid_klienta, int nr_kolejki);
    int czy_oplaca_sie_zmienic_kolejke(int pid_klienta, int aktualny_nr_kolejki);
};

// -- FUNKCJE --

int randomTime(int time);
int randomTimeWithRange(int min, int max);
int pobierz_ilosc_wiadomosci(int msqid);
int findInexOfPid(int pid, StanDyskontu * stan);
int wyswietl_cene_produktu(const char * produkt);
