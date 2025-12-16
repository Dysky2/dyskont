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
#include <limits.h>

#define MAX_DATA_SIZE 200

#define ILOSC_KOLJEJEK 3
#define MAX_DLUGOSC_KOLEJEK 100
#define ILOSC_KAS 8

#define CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA 10
#define CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA 15

#define SEMAFOR_SAMOOBSLUGA 0
#define SEMAFOR_STAC1 1
#define SEMAFOR_STAC2 2
#define SEMAFOR_ILOSC_KLIENTOW 3
#define SEMAFOR_ILOSC_KAS 4

const double simulation_speed = 1.0;
const int simulation_time = 20;
const int startowa_ilosc_kas = 3;


// TODO ZAMIAN NAZWY NA Kasy
struct kasy {
    int pid_kasy[8];
    int status[8]; // 0 -> kasa zamknieta 1 -> kasa otwarta 2-> kasa zajeta
    int kolejka_pid[ILOSC_KOLJEJEK][MAX_DLUGOSC_KOLEJEK];
    int dlugosc_kolejki[ILOSC_KOLJEJEK];

    float sredni_czas_obslugi[3]; // 0 czas kasy samooblugowe, 1 -> czas kasy stacjonranerej_1, 2 -> czas kasy stacjonranerej_2
};

struct Obsluga {
    long int mtype;
    int powod; // 1 -> Sprawdzenie wieku, 2 -> popsuta kasa
    int kasa_id;
    int wiek_klienta;
    int pelnoletni; // -1 -> osobo niepelnetnia 0 -> pelnoletni
};

struct Klient {
    long int mtype;
    int klient_id;
    int nrKasy;
    int wiek;
    int ilosc_produktow;
    char lista_produktow[MAX_DATA_SIZE];
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

inline int checkError(int result, const char * msg) {
    if (result == -1) {
        perror(msg);
        exit(1);
    }
    return result;
};

inline int randomTime(int time) {
    return rand() % time; 
};

inline int randomTimeWithRange(int min, int max) {
    return rand() % (max - min + 1) + min; 
};

struct AtomicLogger {
    std::stringstream bufor;

    ~AtomicLogger() {
        // bufor << "\n";
        std::string tresc = bufor.str();
        write(1, tresc.c_str(), tresc.length());
    }

    template<typename T>
    AtomicLogger& operator<<(const T& wartosc) {
        bufor << wartosc;
        return *this;
    }
};

#define komunikat AtomicLogger()

class Kolejka {
private: 
    kasy * lista_kas;

public:
    Kolejka(kasy * lista_kas) {
        this->lista_kas = lista_kas;
    }

    void dodaj_do_kolejki(int pid_klienta, int nr_kolejki) {
        int index = lista_kas->dlugosc_kolejki[nr_kolejki];
        if(index < 0 || index >= 100) {
            return;
        }
        lista_kas->kolejka_pid[nr_kolejki][index] = pid_klienta;
        lista_kas->dlugosc_kolejki[nr_kolejki]++;
    }

    void usun_z_kolejki(int pid_klienta, int nr_kolejki) {
        if(nr_kolejki < 0 || nr_kolejki >= 3) {
            perror("blad usuniecia kolejki");
            return;
        }
        int ilosc_osob_w_kolejce = lista_kas->dlugosc_kolejki[nr_kolejki];
        if(ilosc_osob_w_kolejce > 0) {
            int index_klienta = -1;
            for(int i=0; i< ilosc_osob_w_kolejce; i++) {
                if(pid_klienta == lista_kas->kolejka_pid[nr_kolejki][i]) {
                    index_klienta = i;
                    break;
                }
            }
            if(index_klienta != -1) {
                for(int j = index_klienta; j < ilosc_osob_w_kolejce - 1; j++ ) {
                    lista_kas->kolejka_pid[nr_kolejki][j] = lista_kas->kolejka_pid[nr_kolejki][j + 1];
                }
                lista_kas->kolejka_pid[nr_kolejki][ilosc_osob_w_kolejce - 1] = 0;
                lista_kas->dlugosc_kolejki[nr_kolejki]--;
            }
        }
    }

    void wyswietl_kolejke(int nr_kolejki) {
        std::stringstream bufor;
        for(int i=0; i < lista_kas->dlugosc_kolejki[nr_kolejki]; i++) {
            bufor << lista_kas->kolejka_pid[nr_kolejki][i] << " ";
        }
        komunikat << bufor.str() << "\n\n";
        bufor.clear();
    }

    int czy_jestem_pierwszy(int pid_klienta, int nr_kolejki) {
        if(pid_klienta == lista_kas->kolejka_pid[nr_kolejki][0]) {
            return 1;
        }
        return 0;
    }

    // szuka w podanym numerze kolejki swojego id i zwraca pozycje na której się znajduje
    int znajdz_moja_pozycje_w_kolejce(int pid_klienta, int nr_kolejki) {
        int pozycja = -1;
        for(int i=0;i < lista_kas->dlugosc_kolejki[nr_kolejki]; i++) {
            if(pid_klienta == lista_kas->kolejka_pid[nr_kolejki][i]) {
                pozycja = i;
                break;
            }
        }
        return pozycja;
    }

    int czy_oplaca_sie_zmienic_kolejke(int pid_klienta, int aktualny_nr_kolejki) {

        int pozycja = znajdz_moja_pozycje_w_kolejce(pid_klienta, aktualny_nr_kolejki);
        if(pozycja == -1) return -1;

        float najkrotszy_czas = pozycja * lista_kas->sredni_czas_obslugi[aktualny_nr_kolejki];

        float czas_kolejki_do_ktorej_chce_przejsc = -1;
        int najepszy_wybor = -1;

        if(aktualny_nr_kolejki == 0) {
            for(int i=6 ; i < 8; i++) {

                int nr_koleki = i == 6 ? 1 : 2;

                czas_kolejki_do_ktorej_chce_przejsc = lista_kas->sredni_czas_obslugi[nr_koleki] * lista_kas->dlugosc_kolejki[nr_koleki];

                if(lista_kas->status[i] == 1 && (czas_kolejki_do_ktorej_chce_przejsc + 10 < najkrotszy_czas)) {
                    najkrotszy_czas = czas_kolejki_do_ktorej_chce_przejsc;
                    najepszy_wybor = nr_koleki;
                }
            }

        } 
        else if(aktualny_nr_kolejki == 1) {
            czas_kolejki_do_ktorej_chce_przejsc = lista_kas->sredni_czas_obslugi[2] * lista_kas->dlugosc_kolejki[2];

            if(lista_kas->status[7] == 1 && (czas_kolejki_do_ktorej_chce_przejsc + 10 < najkrotszy_czas) ) {
                najkrotszy_czas = czas_kolejki_do_ktorej_chce_przejsc;
                najepszy_wybor = 2;
            }
        }
        return najepszy_wybor;
    }
};


int findInexOfPid(int pid, kasy * lista) {
    int res = -1;

    for(int i = 0; i < 8; i++) {
        if( lista->pid_kasy[i] == pid ) {
            res = i;
        }
    }
    return res;
};

const std::string kategorieProduktow[10] = {
    "OWOCE",
    "WARZYWA",
    "PIECZYWO",
    "NABIAL",
    "ALKOHOL",
    "WEDLINY",
    "MAKARONY",
    "NAPOJE",
    "MIESO",
    "RYBY"
};

const std::vector<std::vector<int>> products_price = {
    {3, 6, 5, 15, 20},
    {3, 2, 8, 6},
    {5, 1, 4},
    {4, 12, 3, 8, 5, 4},
    {5, 30, 40, 90},
    {15, 25, 12, 10, 18},
    {6, 6, 6},
    {2, 7, 6, 5},
    {24, 26, 45, 19},
    {70, 45, 35}
};

const std::vector<std::vector<std::string>> products = {
    {"Jablko", "Banan", "Gruszka", "Truskawka", "Arbuz"},
    {"Marchew", "Ziemniak", "Pomidor", "Ogorek" },
    {"Chleb Razowy", "Bulka Kajzerka", "Bagietka"},
    {"Mleko", "Ser Zolty", "Jogurt Naturalny", "Maslo", "Smietana", "Kefir" },
    {"Piwo", "Wino", "Wodka", "Whisky"},
    {"Szynka Babuni", "Kielbasa Slaska", "Kabanosy", "Parowki", "Salami" },
    {"Spaghetti", "Swiderki", "Penne"},
    { "Woda Niegazowana", "Cola", "Sok Pomaranczowy", "Zielona Herbata"},
    {"Piers z Kurczaka", "Schab", "Wolowina", "Mielone"},
    { "Losos", "Dorsz", "Pstrag"}
};


inline int wyswietl_cene_produktu(const char * produkt) {
    for(int i=0;i < products.size(); i++) {
        for(int j=0; j < products[i].size(); j++) {
            if( strcmp(products[i][j].c_str(), produkt) == 0) {
                return products_price[i][j];
            }
        }
    }
    return 0;
};