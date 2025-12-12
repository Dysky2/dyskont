#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    srand(time(0) + getpid());
    komunikat << "Tutaj kasa witam" << "\n";

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int semid_kolejki = atoi(argv[4]);
    int msqid_kolejka_obsluga = atoi(argv[5]);

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

    while(1) {
        Klient klient;
        checkError( msgrcv(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long int), 1, 0), "Blad odebrania wiadomosci" );
        
        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }

        sleep(randomTime(12));

        komunikat << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Ilosc produktow " << klient.ilosc_produktow << " O typie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << "\n";
        
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
            if(randomTimeWithRange(1, 100) > 90) {
                komunikat << "Waga towaru sie nie zgadza, prosze poczekac na obsluge \n";
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

        if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
            komunikat << "KASA probuje odjac z kolejki gdzie jest 0" << "\n" << "\n"; 
        } else {
            zmien_wartosc_kolejki(semid_kolejki, lista_kas, klient.nrKasy , -1);
        }

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            msgsnd(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long int), 0);
        }

        komunikat << "Ilosc ludzi w kolejce OD KASY " << lista_kas->liczba_ludzi[0] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASY " << lista_kas->liczba_ludzi[1] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASY " << lista_kas->liczba_ludzi[2] << "\n";
  
    }

    komunikat << "Koniec kasy " << "\n";
    exit(0);
}