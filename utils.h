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

#define MAX_DATA_SIZE 200

const double simulation_speed = 1.0;
const int simulation_time = 10;

inline int checkError(int result, const char * msg) {
    if (result == -1) {
        perror(msg);
        exit(1);
    }

    return result;
}

inline int randomTime(int time) {
    return rand() % time; 
}

inline int randomTimeWithRange(int min, int max) {
    return rand() % (max - min + 1) + min; 
}

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

struct Klient {
    long mtype;
    int klient_id;
    int nrKasy;
    int wiek;
    int ilosc_produktow;
    char lista_produktow[MAX_DATA_SIZE];
};

struct kasy {
    int pid_kasy[8];
    int status[8];
    int liczba_ludzi[3];
};

struct Obsluga {
    long int mtype;
    int powod; // 1 -> Sprawdzenie wieku, 2 -> popsuta kasa
    int kasa_id;
    int wiek_klienta;
    int pelnoletni; // -1 -> osobo niepelnetnia 0 -> pelnoletni
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

inline void zmien_wartosc_kolejki(int semid, kasy * lista_kas, int nrKolejki, int operacja) {
    struct sembuf OperacjaP = {0, -1, SEM_UNDO};
    checkError( semop(semid, &OperacjaP, 1), "Blad obnizenia semafora" );

    lista_kas->liczba_ludzi[nrKolejki] += operacja;

    struct sembuf OperacjaV = {0, 1, SEM_UNDO};
    checkError( semop(semid, &OperacjaV, 1), "Blad podniesienia semafora" );
}

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
}