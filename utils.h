#pragma once

#include <cstdio> 
#include <cstdlib>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string>
#include <vector>

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

struct klientWzor {
    long mtype;
    int klient_id;
    int ilosc_produktow;
    char lista_produktow[10][20];
};

const std::string kategorieProduktow[10] = {
    "OWOCE",
    "WARZYWA",
    "PIECZYWO",
    "NABIAL",
    "ALHOHOL",
    "WEDLINY",
    "MAKARONY",
    "NAPOJE",
    "MIESO",
    "RYBY"
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