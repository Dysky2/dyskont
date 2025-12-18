#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kasa_otwarta = 1; 

int shmid_kasy = 0;

void zacznij_prace(int) { }

void wstrzymaj_kase(int) { }

void przerwij_prace(int) {}

void zamknij_kase(int) {
    czy_kasa_otwarta = 0;
}

void otworz_kase_stac2(int) {
    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);
    if(stan_dyskontu->status_kasy[7] == 1) {
    } else {
        komunikat << "Otwieram kase stacjonarna 2\n";
        stan_dyskontu->status_kasy[7] = 1;
    }
}

int main(int, char * argv[]) {
    komunikat << "Otwieram kase stacjonarna " << getpid() << "\n";

    signal(SIGALRM, wstrzymaj_kase);
    signal(SIGUSR1, otworz_kase_stac2);
    signal(SIGUSR2, przerwij_prace);
    // signal(SIGCONT, zacznij_prace);
    signal(SIGTERM, zamknij_kase);

    int sem_id = atoi(argv[1]);
    shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);

    int nr_kasy = checkError(findInexOfPid(getpid(), stan_dyskontu), "Blad: nie znaleziono pidu kasy\n");

    while(czy_kasa_otwarta) {
        int id_kasy = nr_kasy == 6 ? msqid_kolejka_stac1 : msqid_kolejka_stac2;

        if(stan_dyskontu->status_kasy[nr_kasy] == 0) {
            if(pobierz_ilosc_wiadomosci(id_kasy) <= 0) {
                alarm(2);
                pause();
                alarm(0);
                continue;
            }
        } 
        komunikat << "KASJER NORMALNIE SMIGA " << nr_kasy << "\n\n\n";

        Klient klient;
        memset(&klient, 0, sizeof(Klient));

        // Po ilu ma sie kasa wylaczyc -- domyslnie 30
        alarm(10);  
        int status = msgrcv(id_kasy, &klient, sizeof(Klient) - sizeof(long int), 1, MSG_NOERROR);
        alarm(0);

        if(klient.ilosc_produktow == -1 || klient.klient_id==getpid()) {
            break;
        }

        if(status == -1) {
            if(errno == EINTR && (stan_dyskontu->dlugosc_kolejki[nr_kasy == 6 ? 1 : 2] > 0)) {
                continue;
            }
            if(errno == EIDRM || errno == EINVAL) break;
            if(errno == EINTR) {
                komunikat << "Zamykam kase o pidzie: " << getpid() << "\n";
                struct sembuf operacjaP = {SEMAFOR_ILOSC_KAS, -1, SEM_UNDO};
                semop(sem_id, &operacjaP, 1);
                stan_dyskontu->status_kasy[nr_kasy] = 0;
                continue;
            }
        }

        komunikat << "[KASJER-" << getpid() << "-" << klient.nrKasy << "]" << " ODEBRANO KOMUNIKAT W KASJERZE " << klient.klient_id << " Z TEJ STRONY: " << getpid() << " nrkasy to " << klient.nrKasy << "\n";

        sleep(randomTime(15));

        if(!czy_kasa_otwarta) continue;
        
        stringstream paragon;
        time_t t = time(0);
        struct tm * timeinfo = localtime(&t);
        char bufor_czasu[80];
        strftime(bufor_czasu, 80, "%H:%M:%S %d.%m.%Y", timeinfo);

        paragon << "\n";
        paragon << ".==================================." << "\n";
        paragon << "|             Dyskont              |" << "\n";
        paragon << "|         ul. Warszawska 93        |" << "\n";
        paragon << "|          32-000 Kraków           |" << "\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "|    Paragon fiskalny nr:  " << left << setw(8) << randomTime(10000) << "|\n";
        paragon << "|    ID kasy stacjonarnej: " << left << setw(8) << getpid() << "|\n";
        paragon << "|    Kasa stacjonrana nr: " << left << setw(9) << klient.nrKasy << "|\n";
        paragon << "|    Data: " << left << setw(24) << bufor_czasu << "|\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "| Towar                    Wartosc |" << "\n";
        paragon << "|                                  |" << "\n";

        int aktualna_pozycja = 0, suma = 0;
        for(int i=0;i < klient.ilosc_produktow;i++) {
            char * produkt = klient.lista_produktow + aktualna_pozycja;
            if( strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0 ) {
                if(klient.wiek >= 18) {
                    int cena = wyswietl_cene_produktu(produkt);
                    suma += cena;
                    paragon << "| " << left << setw(21) << produkt 
                        << right << setw(8) << cena << " zl |";
                    if (i < klient.ilosc_produktow - 1) {
                        paragon << "\n";
                    }
                }
            } else {
                int cena = wyswietl_cene_produktu(produkt);
                suma += cena;
                paragon << "| " << left << setw(21) << produkt 
                    << right << setw(8) << cena << " zl |";
                if (i < klient.ilosc_produktow - 1) {
                    paragon << "\n";
                }
            }
            aktualna_pozycja += strlen(produkt) + 1;
        }

        paragon << "\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "|                                  |" << "\n";
        paragon << "| " << left << setw(10) << "Suma PLN" << right << setw(19) << suma << " zl |\n";
        paragon << "|                                  |" << "\n";
        paragon << "|==================================|" << "\n";
        paragon << "|      POTWIERDZENIE PLATNOSCI     |" << "\n";
        paragon << "|          KARTA PLATNICZA         |" << "\n";
        paragon << "|                                  |" << "\n";
        paragon << "| MERCH ID: " << left << setw(23) << (randomTimeWithRange(10000000, 99999999)) << "|\n";
        paragon << "| TERM ID:  " << left << setw(23) << ("T000" + to_string(klient.nrKasy)) << "|\n";
        paragon << "|                                  |\n";
        paragon << "| KARTA:    " << left << setw(23) << "VISA CONTACTLESS" << "|\n";
        paragon << "| PAN:      ************" << left << setw(11) << (1000 + rand() % 9000) << "|\n";
        paragon << "| AID:      A0000000031010         |\n";
        paragon << "|                                  |\n";
        paragon << "| SPRZEDAZ                         |\n";
        paragon << "| KOD AUT.: " << left << setw(23) << (100000 + rand() % 900000) << "|\n";
        paragon << "|                                  |\n";
        paragon << "| KWOTA:    " << right << setw(19) << suma << " zl |\n";
        paragon << "|                                  |\n";
        paragon << "|      TRANSAKCJA ZAAKCEPTOWANA    |\n";
        paragon << "|          ZACHOWAJ WYDRUK         |\n";
        paragon << "|                                  |\n";
        paragon << "| ||| || ||| | ||| || || ||| | ||| |\n";
        paragon << "| ||| || ||| | ||| || || ||| | ||| |\n";
        paragon << "| ||| || ||| | ||| || || ||| | ||| |\n";
        paragon << "|    0023    " << left << setw(4) << klient.klient_id << "      9912       |\n";
        paragon << "|                                  |\n";
        paragon << "| DZIEKUJEMY I ZAPRASZAMY PONOWNIE |\n";
        paragon << "'=================================='" << "\n"; 
        komunikat << paragon.str() << "\n";

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            msgsnd(id_kasy, &klient, sizeof(Klient) - sizeof(long int), 0);
        }

        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << stan_dyskontu->dlugosc_kolejki[0] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << stan_dyskontu->dlugosc_kolejki[1] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << stan_dyskontu->dlugosc_kolejki[2] << "\n";
    }

    komunikat  << "[KASJER-" << getpid() << "]" << " Zamykam kase stacjonarna" << "\n" << "\n";
    exit(0);
}