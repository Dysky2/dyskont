#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kasa_otwarta = 1; 

void zamknij_kase(int) {
    czy_kasa_otwarta = 0;
}

void przerwij_prace(int) {}

void zacznij_prace(int) { }

int main(int argc, char * argv[]) {
    srand(time(0) + getpid());

    signal(SIGCONT, zacznij_prace);
    signal(SIGTERM, zamknij_kase);
    signal(SIGUSR2, przerwij_prace);

    if(argc <= 3) {
        perror("Podano za malo argumentow dla kasy");
        exit(1);
    }

    int sem_id = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_obsluga = atoi(argv[4]);

    komunikat << "[KASA-" << getpid() << "]" << " Witam zapraszamy" << "\n";

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL , 0);

    struct sembuf operacjaP = {SEMAFOR_OBSLUGA, -1, SEM_UNDO};
    struct sembuf operacjaV = {SEMAFOR_OBSLUGA, 1, SEM_UNDO};

    stringstream bufor;
    for(int i=0;i<8;i++) {
        bufor << stan_dyskontu->pid_kasy[i] << " ";
    }
    for(int i=0;i<8;i++) {
        bufor << stan_dyskontu->status_kasy[i] << " ";
    }
    bufor << "\n";
    komunikat << bufor.str();
    bufor.clear();

    int nr_kasy = checkError(findInexOfPid(getpid(), stan_dyskontu), "Blad: nie znaleziono pidu kasy\n");

    while(czy_kasa_otwarta) {
        if(stan_dyskontu->status_kasy[nr_kasy] == 0) {
            pause();
            continue;
        }
        Klient klient;
        memset(&klient, 0, sizeof(Klient)); 
        int status = msgrcv(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 1, MSG_NOERROR);

        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }

        if(status == -1) {
            if(errno == EINTR) {
                continue;
            }
        }

        komunikat << "[KASA-" << getpid() << "-" <<klient.nrKasy << "]" << " ODEBRANO KOMUNIKAT " << klient.klient_id << " Ilosc produktow " << klient.ilosc_produktow << " O typie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << "\n";
        
        sleep(randomTime(15));
        
        if(!czy_kasa_otwarta) {
            continue;
        }

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
        paragon << "|    ID kasy samoobslugowej: " << left << setw(6) << getpid() << "|\n";
        paragon << "|    Kasa samoobslugowa nr: " << left << setw(7) << klient.nrKasy << "|\n";
        paragon << "|    Data: " << left << setw(24) << bufor_czasu << "|\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "| Towar                    Wartosc |" << "\n";
        paragon << "|                                  |" << "\n";

        int aktualna_pozycja = 0, suma = 0;
        for(int i=0;i < klient.ilosc_produktow;i++) {

            // 2% szans ze kasa utknie
            if(randomTimeWithRange(1, 100) > 98) {

                if(semop(sem_id, &operacjaP, 1) == -1) {
                    if(errno == EINTR) {
                        paragon.clear();
                        break;
                    } else{ 
                        perror("Bledne opuszczenie semafor z OBSLUGI");
                        exit(EXIT_FAILURE);
                    }
                }

                komunikat << "[KASA-" << getpid() << "]" << " Waga towaru sie nie zgadza, prosze poczekac na obsluge \n";
                Obsluga obsluga = {1, 2, getpid(), -1, -1};

                if(czy_kasa_otwarta) {
                    checkError( 
                        msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0),
                        "Blad wyslania komuniaktu to obslugi o sprawdzenie pelnoletnosci przez kase"
                    );
                } else {
                    checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
                    break;
                }

                int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);

                if(rcvStatus == -1 || !czy_kasa_otwarta) {
                    if(errno == EINTR || !czy_kasa_otwarta) {
                        paragon.clear();
                        checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
                        break;
                    } else {
                        perror("Blad odebrania wiadomosci od obslugi");
                        exit(1);
                    }
                }

                checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
            }

            char * produkt = klient.lista_produktow + aktualna_pozycja;
            if( strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0 ) {
                    
                if(semop(sem_id, &operacjaP, 1) == -1) {
                    if(errno == EINTR) {
                        paragon.clear();
                        break;
                    } else{ 
                        perror("Bledne opuszczenie semafor z OBSLUGI");
                        exit(EXIT_FAILURE);
                    }
                }

                Obsluga obsluga = {1,1,getpid(), klient.wiek, -1};
                if(czy_kasa_otwarta) {
                    checkError( 
                        msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0),
                        "Blad wyslania komuniaktu to obslugi o sprawdzenie pelnoletnosci przez kase"
                    );
                } else {
                    checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
                    break;
                }

                int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);

                if(rcvStatus == -1 || !czy_kasa_otwarta) {
                    if(errno == EINTR || !czy_kasa_otwarta) {
                        paragon.clear();
                        checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
                        break;
                    } else {
                        perror("Blad odebrania wiadomosci od obslugi");
                        exit(1);
                    }
                }

                if(obsluga.pelnoletni == 0) {
                    int cena = wyswietl_cene_produktu(produkt);
                    suma += cena;
                    paragon << "| " << left << setw(21) << produkt 
                        << right << setw(8) << cena << " zl |";
                    if (i < klient.ilosc_produktow - 1) {
                        paragon << "\n";
                    }
                }
                
                checkError(semop(sem_id, &operacjaV, 1), "Bledne podniesie semafora od OBSLUGI");
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

        if(!czy_kasa_otwarta) {
            paragon.clear();
            continue;
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
        paragon << "| NR KARTY:      ************" << left << setw(6) << (1000 + rand() % 9000) << "|\n";
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
        paragon << "|    0023    " << left << setw(5) << klient.klient_id << "      9912       |\n";
        paragon << "|                                  |\n";
        paragon << "| DZIEKUJEMY I ZAPRASZAMY PONOWNIE |\n";
        paragon << "'=================================='" << "\n"; 

        komunikat << paragon.str() << "\n";

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            msgsnd(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 0);
        }
    }

    komunikat << "[KASA-" << getpid() << "]" << " Koniec" << "\n";
    exit(0);
}