#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kasa_otwarta = 1; 

void zamknij_kase(int sigint) {
    czy_kasa_otwarta = 0;
}

void zacznij_prace(int sigint) { }

int main(int argc, char * argv[]) {
    srand(time(0) + getpid());

    signal(SIGCONT, zacznij_prace);
    signal(SIGTERM, zamknij_kase);

    komunikat << "[KASA-" << getpid() << "]" << " Witam zapraszamy" << "\n";

    int sem_id = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_obsluga = atoi(argv[4]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL , 0);

    stringstream bufor;
    for(int i=0;i<8;i++) {
        bufor << lista_kas->pid_kasy[i] << " ";
    }
    for(int i=0;i<8;i++) {
        bufor << lista_kas->status[i] << " ";
    }
    bufor << "\n";
    komunikat << bufor.str();
    bufor.clear();

    while(czy_kasa_otwarta) {
        if(lista_kas->status[findInexOfPid(getpid(), lista_kas)] == 0) {
            pause();
            continue;
        }
        Klient klient;
        memset(&klient, 0, sizeof(Klient)); 
        checkError( msgrcv(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 1, MSG_NOERROR), "Blad odebrania wiadomosci" );
        
        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }

        komunikat << "[KASA-" << getpid() << "-" <<klient.nrKasy << "]" << " ODEBRANO KOMUNIKAT " << klient.klient_id << " Ilosc produktow " << klient.ilosc_produktow << " O typie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << "\n";
        sleep(randomTime(15));
        
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
        paragon << "|    Kasa samoobsługowa nr: " << left << setw(7) << klient.nrKasy << "|\n";
        paragon << "|    Data: " << left << setw(24) << bufor_czasu << "|\n";
        paragon << "|----------------------------------|" << "\n";
        paragon << "| Towar                    Wartosc |" << "\n";
        paragon << "|                                  |" << "\n";

        int aktualna_pozycja = 0, suma = 0;
        for(int i=0;i < klient.ilosc_produktow;i++) {

            // 2% szans ze kasa sie 
            if(randomTimeWithRange(1, 100) > 98) {
                komunikat << "[KASA-" << getpid() << "]" << " Waga towaru sie nie zgadza, prosze poczekac na obsluge \n";
                Obsluga obsluga = {1, 2, getpid()};

                checkError( 
                    msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0),
                    "Blad wyslania komuniaktu to obslugi o sprawdzenie pelnoletnosci przez kase"
                );

                msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);
            }


            char * produkt = klient.lista_produktow + aktualna_pozycja;
            if( strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0 ) {
                    
                Obsluga obsluga = {1,1,getpid(), klient.wiek};

                checkError( 
                    msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0),
                    "Blad wyslania komuniaktu to obslugi o sprawdzenie pelnoletnosci przez kase"
                );

                msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), getpid(), 0);

                if(obsluga.pelnoletni == 0) {
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
            msgsnd(msqid_kolejka_samo, &klient, sizeof(Klient) - sizeof(long int), 0);
        }
    }

    komunikat << "[KASA-" << getpid() << "]" << " Koniec" << "\n";
    exit(0);
}