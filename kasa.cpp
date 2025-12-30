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

    prctl(PR_SET_PDEATHSIG, SIGTERM);

    signal(SIGUSR1, zacznij_prace);
    signal(SIGINT, zamknij_kase);
    signal(SIGTERM, zamknij_kase);
    signal(SIGUSR2, przerwij_prace);

    if(argc <= 3) {
        showError("Podano za malo argumentow dla kasy");
        exit(EXIT_FAILURE);
    }

    utworz_grupe_semaforowa();

    int sem_id = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_obsluga = atoi(argv[4]);
    string nazwa_katalogu = argv[5];

    ustaw_nazwe_katalogu(nazwa_katalogu);

    komunikat << "[KASA-" << getpid() << "]" << " Witam zapraszamy" << "\n";

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL , 0);

    if(stan_dyskontu == (void*) -1) {
        showError("[KASA] Bledne podlaczenie pamieci dzielonej stanu dyskontu");
        exit(EXIT_FAILURE);
    }

    stringstream bufor;
    bufor << "[KASA] ";
    for(int i=0;i<8;i++) {
        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
        bufor << stan_dyskontu->pid_kasy[i] << " ";
        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);
    }
    for(int i=0;i<8;i++) {
        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);  
        bufor << stan_dyskontu->status_kasy[i] << " ";
        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);
    }
    bufor << "\n";
    komunikat << bufor.str();
    bufor.clear();

    int nr_kasy = checkError(findInexOfPid(getpid(), stan_dyskontu), "Blad: nie znaleziono pidu kasy\n");

    while(czy_kasa_otwarta) {
        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
        int status_kasy = stan_dyskontu->status_kasy[nr_kasy];
        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);
        if(status_kasy == 0) {
            pause();
            continue;
        }

        Klient klient;
        memset(&klient, 0, sizeof(Klient)); 
        int status = msgrcv(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 1, 0);

        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) break;

        if(status == -1) {
            if(errno == EINTR) continue;
            else break;
        }

        komunikat << "[KASA-" << getpid() << "-" << klient.nrKasy << "]" << " ODEBRANO KOMUNIKAT " << klient.klient_id << " Ilosc produktow " << klient.ilosc_produktow << " O typie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << "\n";
        
        sleep(randomTime(CZAS_KASOWANIA_PRODUKTOW));
        
        if(!czy_kasa_otwarta) continue;

        stringstream paragon;

        paragon << "\n";
        paragon << ".==================================." << "\n";
        paragon << "|             Dyskont              |" << "\n";
        paragon << "|         ul. Warszawska 93        |" << "\n";
        paragon << "|          32-000 Kraków           |" << "\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "|    Paragon fiskalny nr:  " << left << setw(8) << randomTime(10000) << "|\n";
        paragon << "|    ID kasy: " << left << setw(21) << getpid() << "|\n";
        paragon << "|    Kasa samoobslugowa nr: " << left << setw(7) << klient.nrKasy << "|\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "| Towar                    Wartosc |" << "\n";
        paragon << "|                                  |" << "\n";

        int aktualna_pozycja = 0, suma = 0, czy_klient_zostal_sprawdzony = 0, czy_jest_pelnoletni = 0 , ile_produktow_odlozonych = 0;
        for(int i=0;i < klient.ilosc_produktow;i++) {

            if(!czy_kasa_otwarta) break; 

            // 2% szans ze kasa utknie
            if(randomTimeWithRange(1, 100) > 98) {

                operacja_p(sem_id, SEMAFOR_OBSLUGA);

                komunikat << "[KASA-" << getpid() << "]" << " Waga towaru sie nie zgadza, prosze poczekac na obsluge \n";
                Obsluga obsluga = {1, 2, getpid(), -1, -1};

                if(czy_kasa_otwarta) {
                    int send_status = msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0);

                    if(send_status == -1) {
                        if (errno == EINTR) {
                            operacja_v(sem_id, SEMAFOR_OBSLUGA);
                            i--;
                            continue;
                        } else {
                            operacja_v(sem_id, SEMAFOR_OBSLUGA);
                            showError("Bledne wyslanie wiadomosci do oblugi o sprawdzenie waga");
                            exit(EXIT_FAILURE);
                        }
                    }

                } else {
                    operacja_v(sem_id, SEMAFOR_OBSLUGA);
                    break;
                }

                int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);

                if(!czy_kasa_otwarta) {
                    operacja_v(sem_id, SEMAFOR_OBSLUGA);
                    break;
                }

                if(rcvStatus == -1) {
                    if(errno == EINTR && !czy_kasa_otwarta) {
                        operacja_v(sem_id, SEMAFOR_OBSLUGA);
                        break;
                    }
                    if(errno == EINTR) {
                        operacja_v(sem_id, SEMAFOR_OBSLUGA);
                        i--;
                        continue;
                    } else {
                        showError("Blad odebrania wiadomosci od obslugi");
                        exit(EXIT_FAILURE);
                    }
                }

                operacja_v(sem_id, SEMAFOR_OBSLUGA);
            }

            char * produkt = klient.lista_produktow + aktualna_pozycja;


            if(!czy_klient_zostal_sprawdzony) {
                if( strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0 ) {
                    
                    operacja_p(sem_id, SEMAFOR_OBSLUGA);

                    Obsluga obsluga = {1,1,getpid(), klient.wiek, -1};
                    if(czy_kasa_otwarta) {
                        int send_status = msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0);

                        if(send_status == -1) {
                            if (errno == EINTR) {
                                operacja_v(sem_id, SEMAFOR_OBSLUGA);
                                i--;
                                continue;
                            } else {
                                operacja_v(sem_id, SEMAFOR_OBSLUGA);
                                showError("Bledne wyslanie wiadomosci do oblugi o sprawdzenie alkoholu");
                                exit(EXIT_FAILURE);
                            }
                        }
                    } else {
                        operacja_v(sem_id, SEMAFOR_OBSLUGA);
                        break;
                    }

                    int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);

                    if(!czy_kasa_otwarta) {
                        operacja_v(sem_id, SEMAFOR_OBSLUGA);
                        break;
                    }

                    if(rcvStatus == -1) {
                        if(errno == EINTR && !czy_kasa_otwarta) {
                            operacja_v(sem_id, SEMAFOR_OBSLUGA);
                            break;
                        }
                        if(errno == EINTR) {
                            operacja_v(sem_id, SEMAFOR_OBSLUGA);
                            i--;
                            continue;
                        } else {
                            showError("Blad odebrania wiadomosci od obslugi");
                            exit(EXIT_FAILURE);
                        }
                    }

                    czy_klient_zostal_sprawdzony = 1;

                    if(obsluga.pelnoletni) {
                        czy_jest_pelnoletni = 1;
                    }
                    
                    operacja_v(sem_id, SEMAFOR_OBSLUGA);
                } 
            }

            if(strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0) {
                if(czy_jest_pelnoletni) {
                    int cena = wyswietl_cene_produktu(produkt);
                    suma += cena;
                    paragon << "| " << left << setw(21) << produkt 
                        << right << setw(8) << cena << " zl |\n";
                } else {
                    komunikat << "[KASA-" << getpid() << "] " << "Produkt wycofany: " << produkt << "\n";
                    ile_produktow_odlozonych++;
                }
            } else {
                int cena = wyswietl_cene_produktu(produkt);
                suma += cena;
                paragon << "| " << left << setw(21) << produkt 
                    << right << setw(8) << cena << " zl |\n";
            }

            aktualna_pozycja += strlen(produkt) + 1;
        }

        if(!czy_kasa_otwarta) {
            paragon.str("");
            paragon.clear();
            continue;
        }

        if(suma == 0) {
            komunikat << "[KASA-" << getpid() << "]" << " klient odlozyl wszystkie produktu\n";
            paragon.str("");
            paragon.clear();

            if(kill(klient.klient_id, 0) == 0) {
                klient.mtype = klient.klient_id;
                klient.ilosc_produktow = klient.ilosc_produktow - ile_produktow_odlozonych;
                msgsnd(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 0);
            }

            continue;
        }
        
        struct tm timeinfo;
        pobierz_aktualny_czas(&timeinfo);
        char bufor_czasu[80];
        strftime(bufor_czasu, 80, "%H:%M:%S %d.%m.%Y", &timeinfo);

        paragon << "|----------------------------------|" << "\n";
        paragon << "|                                  |" << "\n";
        paragon << "| " << left << setw(10) << "Suma PLN" << right << setw(19) << suma << " zl |\n";
        paragon << "|                                  |" << "\n";
        paragon << "|==================================|" << "\n";
        paragon << "|      POTWIERDZENIE PLATNOSCI     |" << "\n";
        paragon << "|          KARTA PLATNICZA         |" << "\n";
        paragon << "|                                  |\n";
        paragon << "| Klient:   " << left << setw(23) << klient.klient_id << "|\n";
        paragon << "| KARTA:    " << left << setw(23) << "VISA CONTACTLESS" << "|\n";
        paragon << "| NR KARTY: ************" << left << setw(11) << (1000 + rand() % 9000) << "|\n";
        paragon << "| Data: " << left << setw(27) << bufor_czasu << "|\n";
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
        paragon << "|    0023    " << left << setw(10) << klient.klient_id << "    9912    |\n";
        paragon << "|                                  |\n";
        paragon << "| DZIEKUJEMY I ZAPRASZAMY PONOWNIE |\n";
        paragon << "'=================================='" << "\n"; 

        komunikat << "[KASA-" << getpid() << "]" << paragon.str() << "\n";

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            klient.ilosc_produktow = klient.ilosc_produktow - ile_produktow_odlozonych;
            msgsnd(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 0);
        }
    }
    
    komunikat << "[KASA-" << getpid() << "]" << " Koniec" << "\n";

    checkError( shmdt(stan_dyskontu), "Blad odlaczenia pamieci stanu" );
    exit(0);
}