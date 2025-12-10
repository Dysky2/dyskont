#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
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

    
    while(1) {
        Klient klient;
        checkError( msgrcv(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long int), 1, 0), "Blad odebrania wiadomosci" );
        
        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }

        wyswietl_produkty(klient.lista_produktow, klient.ilosc_produktow);

        sleep(randomTime(12));

        komunikat << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Ilosc produktow " << klient.ilosc_produktow << " O typie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << "\n";
        
        // TODO dorobic paragon i suma do zaplacenia
        int aktualna_pozycja = 0;
        stringstream bufor;
        for(int i=0;i < klient.ilosc_produktow;i++) {
            char * produkt = klient.lista_produktow + aktualna_pozycja;
            if( strcmp(produkt, "Whisky") == 0 || strcmp(produkt, "Piwo")  == 0 ||  strcmp(produkt, "Wino")  == 0 || strcmp(produkt, "Wodka")  == 0 ) {
                    
                Obsluga obsluga;
                obsluga.wiek_klienta = klient.wiek;
                obsluga.powod=1;
                checkError( 
                    msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(obsluga) - sizeof(long int), IPC_NOWAIT),
                    "Blad wyslania komuniaktu to obslugi o sprawdzenie pelnoletnosci przez kase"
                );

                msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(obsluga) - sizeof(long int), getpid(), 0);

                if(obsluga.pelnoletni == 0) {
                    bufor << produkt;
                    if (i < klient.ilosc_produktow - 1) {
                        bufor << ", ";
                    }
                }
            } else {
                bufor << produkt;
                if (i < klient.ilosc_produktow - 1) {
                    bufor << ", ";
                }
            }


            aktualna_pozycja += strlen(produkt) + 1;
        }
        komunikat << bufor.str() << "\n";

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