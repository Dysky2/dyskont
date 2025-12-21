#include "utils.h"

// -- ZMIENNE GLOBALNE -- 

const double simulation_speed = 1.0;
const int simulation_time = 10;
const int startowa_ilosc_kas = 3;

// -- ZMIENNNE LOKALNE --

int sem_id = -1;
std::string nazwa_katalogu;

// -- Funkcje globalne --

int checkError(int result, const char * msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return result;
};

void pobierz_aktualny_czas(struct tm *aktualny_czas) {
    if(aktualny_czas == NULL) {
        perror("Podano nie prawidlowa strukturte czasu");
        exit(EXIT_FAILURE);
    }

    time_t czas = time(NULL);
    *aktualny_czas = *localtime(&czas);
};

key_t utworz_klucz(const char znak) {
    key_t klucz =  ftok(".", znak);
    if (klucz == -1) {
        perror("Bledne stowrzenia klucza");
        exit(EXIT_FAILURE);
    }
    return klucz;
}

int utworz_grupe_semaforowa() {
    sem_id = checkError(
        semget(utworz_klucz('A'), 8, IPC_CREAT|0600),
        "Blad utworzenia semaforow"
    );
    return sem_id;
}

void ustaw_poczatkowe_wartosci_semaforow(int semid) {
    if(semid < 0) {
        perror("Podano nieprawidlowy semid");
        exit(EXIT_FAILURE);
    }

    checkError(semctl(semid, SEMAFOR_SAMOOBSLUGA, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_SAMOOBSLUGA");
    checkError(semctl(semid, SEMAFOR_STAC1, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_STAC1");
    checkError(semctl(semid, SEMAFOR_STAC2, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_STAC2");
    checkError(semctl(semid, SEMAFOR_ILOSC_KLIENTOW, SETVAL, 0), "Bledne ustawienie wartosci dla semafora SEMAFOR_ILOSC_KLIENTOW");
    checkError(semctl(semid, SEMAFOR_ILOSC_KAS, SETVAL, startowa_ilosc_kas), "Bledne ustawienie wartosci dla semafora SEMAFOR_ILOSC_KAS");
    checkError(semctl(semid, SEMAFOR_OBSLUGA, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_OBSLUGA");
    checkError(semctl(semid, SEMAFOR_LISTA_KLIENTOW, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_LISTA_KLIENTOW");
    checkError(semctl(semid, SEMAFOR_OUTPUT, SETVAL, 1), "Bledne ustawienie wartosci dla semafora SEMAFOR_OUTPUT");
}

void operacja_p(int semId, int semNum) {
    struct sembuf operacjaP;
    operacjaP.sem_num = semNum;
    operacjaP.sem_op = -1;
    operacjaP.sem_flg = SEM_UNDO;
    if(semop(semId, &operacjaP, 1) == -1) {
        if(errno == EINTR) {
            operacja_p(semId, semNum);
        } else {
            char bufor[256];
            snprintf(bufor, 256, "Bledne opnizenie semafora w operacja_p, semid %d, semNUM: %d, errno: %d", semId, semNum, errno);
            perror(bufor);
            exit(EXIT_FAILURE);
        }
    }
}

void operacja_v(int semId, int semNum) {
    struct sembuf operacjaV;
    operacjaV.sem_num = semNum;
    operacjaV.sem_op = 1;
    operacjaV.sem_flg = SEM_UNDO;
    if(semop(semId, &operacjaV, 1) == -1) {
        perror("Bledne opnizenie semafora w operacja_p");
        exit(EXIT_FAILURE);
    }
}

std::string utworz_katalog_na_logi() {
    char bufor_data[64];

    struct tm czas;
    pobierz_aktualny_czas(&czas);

    strftime(bufor_data, 64, "./Logi/%Y%m%d-%H%M%S", &czas);

    int status_katalogu = mkdir(bufor_data, 0700);

    if(status_katalogu == -1) {
        if(errno == EEXIST) {
            perror("Katalog juz istnieje");
        } else {
            perror("Nie mozna utworzyc katalogu");
        }
    }

    nazwa_katalogu = bufor_data;
    return bufor_data;
}

void ustaw_nazwe_katalogu(std::string nazwa) {
    nazwa_katalogu = nazwa;
}

// -- STRUKTURY --

AtomicLogger::AtomicLogger() {
    pobierz_aktualny_czas(&aktualny_czas);
    strftime(bufor_czas, 64, "%H:%M:%S | ", &aktualny_czas);
    bufor << bufor_czas;
}

AtomicLogger::~AtomicLogger() {
    // bufor << "\n";
    std::string nazwa_pliku;
    int nazwa_sie_zgadza = 0;
    std::string str_bufor = bufor.str();

    for(auto c : str_bufor) {
        if(c == '[') {
            nazwa_sie_zgadza = 1;
            continue;
        }
        
        if(nazwa_sie_zgadza) {
            if(c == ']' || c == '-') {
                break;
            }
            nazwa_pliku += tolower(c);
        }
    }

    int fd = -1;
    if(nazwa_pliku.length() > 0) {
        std::string sciezka = nazwa_katalogu + "/" + nazwa_pliku + ".log";
        fd = open(sciezka.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
        checkError(fd, "Bledne otawrce pliku ");
    }
    
    operacja_p(sem_id, SEMAFOR_OUTPUT);
    std::string tresc = bufor.str();
    if(fd != -1 && nazwa_pliku.length() > 0) {
        int file_status = write(fd, tresc.c_str(), tresc.length());
        if(file_status == -1) {
            operacja_v(sem_id, SEMAFOR_OUTPUT);
            perror("Blad zapisu write do pliku");
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
    int status = write(1, tresc.c_str(), tresc.length());
    if(status == -1) {
        operacja_v(sem_id, SEMAFOR_OUTPUT);
        perror("Blad zapisu write");
        exit(EXIT_FAILURE);
    }

    operacja_v(sem_id, SEMAFOR_OUTPUT);
}

// -- KLASY --

ListaKlientow::ListaKlientow(DaneListyKlientow * klienci_pamiec, int pierwsze_wywolanie) {
    this->klienci = klienci_pamiec;
    if(pierwsze_wywolanie) {
        klienci->ilosc = 0;
        for(int i=0; i<MAX_ILOSC_KLIENTOW; i++) klienci->lista_klientow[i] = 0;
    }
}

int ListaKlientow::dodaj_klienta_do_listy(int pid_klienta) {
        if(klienci->ilosc >= 0 && klienci->ilosc < MAX_ILOSC_KLIENTOW) {
            klienci->lista_klientow[klienci->ilosc++] = pid_klienta;
        } else {
            komunikat << "[UTLIS-dodaj-klienta] " << "Za duzo ludzi w skelpie\n";
            return 0;
        }
        return 1;
    }

void ListaKlientow::usun_klienta_z_listy(int pid_klienta) {
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

void ListaKlientow::wyswietl_cala_liste() {
        std::stringstream bufor;
        for(int i=0;i<klienci->ilosc;i++) {
            bufor << klienci->lista_klientow[i] << " ";
        }
        komunikat << bufor.str() << "\n";
        bufor.clear();
}

Kolejka::Kolejka(StanDyskontu * stan_dyskontu) {
    this->stan_dyskontu = stan_dyskontu;
}

void Kolejka::dodaj_do_kolejki(int pid_klienta, int nr_kolejki) {
    int index = stan_dyskontu->dlugosc_kolejki[nr_kolejki];
    if(index < 0 || index >= 100) {
        return;
    }
    stan_dyskontu->kolejka_klientow[nr_kolejki][index] = pid_klienta;
    stan_dyskontu->dlugosc_kolejki[nr_kolejki]++;
}

void Kolejka::usun_z_kolejki(int pid_klienta, int nr_kolejki) {
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

void Kolejka::wyswietl_kolejke(int nr_kolejki) {
    std::stringstream bufor;
    for(int i=0; i < stan_dyskontu->dlugosc_kolejki[nr_kolejki]; i++) {
        bufor << stan_dyskontu->kolejka_klientow[nr_kolejki][i] << " ";
    }
    komunikat << nr_kolejki << ": " << bufor.str() << "\n\n";
    bufor.clear();
}

int Kolejka::czy_jestem_pierwszy(int pid_klienta, int nr_kolejki) {
    if(pid_klienta == stan_dyskontu->kolejka_klientow[nr_kolejki][0]) {
        return 1;
    }
    return 0;
}

// szuka w podanym numerze kolejki swojego id i zwraca pozycje na której się znajduje
int Kolejka::znajdz_moja_pozycje_w_kolejce(int pid_klienta, int nr_kolejki) {
    int pozycja = -1;
    for(int i=0;i < stan_dyskontu->dlugosc_kolejki[nr_kolejki]; i++) {
        if(pid_klienta == stan_dyskontu->kolejka_klientow[nr_kolejki][i]) {
            pozycja = i;
            break;
        }
    }
    return pozycja;
}

int Kolejka::czy_oplaca_sie_zmienic_kolejke(int pid_klienta, int aktualny_nr_kolejki) {

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

int randomTime(int time) {
    return rand() % time; 
};

int randomTimeWithRange(int min, int max) {
    return rand() % (max - min + 1) + min; 
};

int pobierz_ilosc_wiadomosci(int msqid) {
    if(msqid < 0) {
        return -1;
    }

    struct msqid_ds buf;

    if (msgctl(msqid, IPC_STAT, &buf) == -1) {
        char perror_buffor[256];
        snprintf(perror_buffor, 256, "Blad msgctl z odbiorem danych, msqid %d, errno: %d", msqid, errno);
        perror(perror_buffor);
        return -1; 
    }

    return buf.msg_qnum;
}

int findInexOfPid(int pid, StanDyskontu * stan) {
    int res = -1;

    for(int i = 0; i < 8; i++) {
        if( stan->pid_kasy[i] == pid ) {
            res = i;
        }
    }
    return res;
};

int wyswietl_cene_produktu(const char * produkt) {
    for(int i=0;i < (int)products.size(); i++) {
        for(int j=0; j < (int)products[i].size(); j++) {
            if( strcmp(products[i][j].c_str(), produkt) == 0) {
                return products_price[i][j];
            }
        }
    }
    return 0;
};

