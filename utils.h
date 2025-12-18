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
#include <ctime>
#include <limits.h>

// -- DEFINICJE MAKSYMALNYCH WARTOSCI --

#define MAX_DATA_SIZE 200
#define MAX_ILOSC_KLIENTOW 100

// -- DEFINICJE DLA STRUKTURY STANDYSKONTU --

#define ILOSC_KOLJEJEK 3
#define MAX_DLUGOSC_KOLEJEK 100
#define ILOSC_KAS 8
#define CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA 10
#define CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA 15

// -- SEMAFORY --

#define SEMAFOR_SAMOOBSLUGA 0
#define SEMAFOR_STAC1 1
#define SEMAFOR_STAC2 2
#define SEMAFOR_ILOSC_KLIENTOW 3
#define SEMAFOR_ILOSC_KAS 4
#define SEMAFOR_OBSLUGA 5
#define SEMAFOR_LISTA_KLIENTOW 6

// -- ZMIENNE GLOBALNE -- 

const double simulation_speed = 1.0;
const int simulation_time = 40;
const int startowa_ilosc_kas = 3;

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

struct AtomicLogger {
    std::stringstream bufor;

    ~AtomicLogger() {
        // bufor << "\n";
        std::string tresc = bufor.str();
        int status = write(1, tresc.c_str(), tresc.length());
        if(status == -1) {
            perror("Blad zapisu write");
            exit(1);
        }
    }

    template<typename T>
    AtomicLogger& operator<<(const T& wartosc) {
        bufor << wartosc;
        return *this;
    }
};

#define komunikat AtomicLogger()

struct DaneListyKlientow {
    int ilosc;
    int lista_klientow[MAX_ILOSC_KLIENTOW];
};

// -- KLASY --

class ListaKlientow  {
private:
    DaneListyKlientow * klienci;

public:
    ListaKlientow(DaneListyKlientow * klienci_pamiec, int pierwsze_wywolanie) {
        this->klienci = klienci_pamiec;
        if(pierwsze_wywolanie) {
            klienci->ilosc = 0;
            for(int i=0; i<MAX_ILOSC_KLIENTOW; i++) klienci->lista_klientow[i] = 0;
        }
    }

    int dodaj_klienta_do_listy(int pid_klienta) {

        if(klienci->ilosc >= 0 && klienci->ilosc < MAX_ILOSC_KLIENTOW) {
            klienci->lista_klientow[klienci->ilosc++] = pid_klienta;
        } else {
            komunikat << "[UTLIS-dodaj-klienta] " << "Za duzo ludzi w skelpie\n";
            return 0;
        }
        return 1;
    }

    void usun_klienta_z_listy(int pid_klienta) {
        if(pid_klienta < 0) {
            perror("Nie ma klienta o takim pidzie w kolejce");
            return;
        }

        int index_klienta = -1;
        for(int i=0;i< klienci->ilosc;i++) {
            if(pid_klienta == klienci->lista_klientow[i]) {
                index_klienta = i;
                break;
            }
       }

       if(index_klienta != -1) {
            for(int i=index_klienta; i < klienci->ilosc - 1; i++) {
                klienci->lista_klientow[i] = klienci->lista_klientow[i + 1];
            }
            klienci->ilosc--;
            klienci->lista_klientow[klienci->ilosc] = 0;
       }else {
            komunikat << "Nie ma takiego klienta na liscie\n";
       }
    }

    void wyswietl_cala_liste() {

        std::stringstream bufor;
        for(int i=0;i<klienci->ilosc;i++) {
            bufor << klienci->lista_klientow[i] << " ";
        }
        komunikat << bufor.str() << "\n";
        bufor.clear();
    }
};

class Kolejka {
private: 
    StanDyskontu * stan_dyskontu;

public:
    Kolejka(StanDyskontu * stan_dyskontu) {
        this->stan_dyskontu = stan_dyskontu;
    }

    void dodaj_do_kolejki(int pid_klienta, int nr_kolejki) {
        int index = stan_dyskontu->dlugosc_kolejki[nr_kolejki];
        if(index < 0 || index >= 100) {
            return;
        }
        stan_dyskontu->kolejka_klientow[nr_kolejki][index] = pid_klienta;
        stan_dyskontu->dlugosc_kolejki[nr_kolejki]++;
    }

    void usun_z_kolejki(int pid_klienta, int nr_kolejki) {
        if(nr_kolejki < 0 || nr_kolejki >= 3) {
            perror("blad usuniecia kolejki");
            return;
        }
        int ilosc_osob_w_kolejce = stan_dyskontu->dlugosc_kolejki[nr_kolejki];
        if(ilosc_osob_w_kolejce > 0) {
            int index_klienta = -1;
            for(int i=0; i< ilosc_osob_w_kolejce; i++) {
                if(pid_klienta == stan_dyskontu->kolejka_klientow[nr_kolejki][i]) {
                    index_klienta = i;
                    break;
                }
            }
            if(index_klienta != -1) {
                for(int j = index_klienta; j < ilosc_osob_w_kolejce - 1; j++ ) {
                    stan_dyskontu->kolejka_klientow[nr_kolejki][j] = stan_dyskontu->kolejka_klientow[nr_kolejki][j + 1];
                }
                stan_dyskontu->kolejka_klientow[nr_kolejki][ilosc_osob_w_kolejce - 1] = 0;
                stan_dyskontu->dlugosc_kolejki[nr_kolejki]--;
            }
        }
    }

    void wyswietl_kolejke(int nr_kolejki) {
        std::stringstream bufor;
        for(int i=0; i < stan_dyskontu->dlugosc_kolejki[nr_kolejki]; i++) {
            bufor << stan_dyskontu->kolejka_klientow[nr_kolejki][i] << " ";
        }
        komunikat << nr_kolejki << ": " << bufor.str() << "\n\n";
        bufor.clear();
    }

    int czy_jestem_pierwszy(int pid_klienta, int nr_kolejki) {
        if(pid_klienta == stan_dyskontu->kolejka_klientow[nr_kolejki][0]) {
            return 1;
        }
        return 0;
    }

    // szuka w podanym numerze kolejki swojego id i zwraca pozycje na której się znajduje
    int znajdz_moja_pozycje_w_kolejce(int pid_klienta, int nr_kolejki) {
        int pozycja = -1;
        for(int i=0;i < stan_dyskontu->dlugosc_kolejki[nr_kolejki]; i++) {
            if(pid_klienta == stan_dyskontu->kolejka_klientow[nr_kolejki][i]) {
                pozycja = i;
                break;
            }
        }
        return pozycja;
    }

    int czy_oplaca_sie_zmienic_kolejke(int pid_klienta, int aktualny_nr_kolejki) {

        int pozycja = znajdz_moja_pozycje_w_kolejce(pid_klienta, aktualny_nr_kolejki);
        if(pozycja == -1) return -1;

        float najkrotszy_czas = pozycja * stan_dyskontu->sredni_czas_obslugi[aktualny_nr_kolejki];

        float czas_kolejki_do_ktorej_chce_przejsc = -1;
        int najepszy_wybor = -1;

        if(aktualny_nr_kolejki == 0) {
            for(int i=6 ; i < 8; i++) {

                int nr_koleki = i == 6 ? 1 : 2;

                czas_kolejki_do_ktorej_chce_przejsc = stan_dyskontu->sredni_czas_obslugi[nr_koleki] * stan_dyskontu->dlugosc_kolejki[nr_koleki];

                if(stan_dyskontu->status_kasy[i] == 1 && (czas_kolejki_do_ktorej_chce_przejsc + 10 < najkrotszy_czas)) {
                    najkrotszy_czas = czas_kolejki_do_ktorej_chce_przejsc;
                    najepszy_wybor = nr_koleki;
                }
            }

        } 
        else if(aktualny_nr_kolejki == 1) {
            czas_kolejki_do_ktorej_chce_przejsc = stan_dyskontu->sredni_czas_obslugi[2] * stan_dyskontu->dlugosc_kolejki[2];

            if(stan_dyskontu->status_kasy[7] == 1 && (czas_kolejki_do_ktorej_chce_przejsc + 10 < najkrotszy_czas) ) {
                najkrotszy_czas = czas_kolejki_do_ktorej_chce_przejsc;
                najepszy_wybor = 2;
            }

            int jest_samooboslugowa = 0;
            for(int i=0;i<6;i++) {
                if(stan_dyskontu->status_kasy[i] == 1) {
                    jest_samooboslugowa = 1;
                    break;
                }
            }

            czas_kolejki_do_ktorej_chce_przejsc = stan_dyskontu->sredni_czas_obslugi[0] * stan_dyskontu->dlugosc_kolejki[0];
            if(jest_samooboslugowa && (czas_kolejki_do_ktorej_chce_przejsc + 10 < najkrotszy_czas)) {
                najkrotszy_czas = czas_kolejki_do_ktorej_chce_przejsc;
                najepszy_wybor = 0;
            }
        }
        return najepszy_wybor;
    }
};

// -- Vectory --
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

// -- FUNKCJE --

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

inline int pobierz_ilosc_wiadomosci(int msqid) {
    struct msqid_ds buf;

    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        perror("Blad msgctl z odbiorem danych");
        return -1; 
    }

    return buf.msg_qnum;
}

inline int findInexOfPid(int pid, StanDyskontu * stan) {
    int res = -1;

    for(int i = 0; i < 8; i++) {
        if( stan->pid_kasy[i] == pid ) {
            res = i;
        }
    }
    return res;
};

inline int wyswietl_cene_produktu(const char * produkt) {
    for(int i=0;i < (int)products.size(); i++) {
        for(int j=0; j < (int)products[i].size(); j++) {
            if( strcmp(products[i][j].c_str(), produkt) == 0) {
                return products_price[i][j];
            }
        }
    }
    return 0;
};