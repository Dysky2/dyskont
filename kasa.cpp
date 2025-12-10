#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    cout << "Tutaj kasa witam" << endl;

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int semid_kolejki = atoi(argv[4]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL , 0);

    for(int i=0;i<8;i++) {
        cout << lista_kas->pid_kasy[i] << " ";
    }
    for(int i=0;i<8;i++) {
        cout << lista_kas->status[i] << " ";
    }
    cout << endl;

    
    while(1) {
        klientWzor klient;
        checkError( msgrcv(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long int), 1, 0), "Blad odebrania wiadomosci" );
        
        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }

        cout << "ODEBRANO KOMUNIKAT " << klient.klient_id << " O tymie: " << klient.mtype << " Z TEJ STRONY: " << getpid() << endl;
        sleep(randomTime(15));

        if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
            cout << "KASA probuje odjac z kolejki gdzie jest 0" << endl << endl;
        } else {
            zmien_wartosc_kolejki(semid_kolejki, lista_kas, klient.nrKasy , -1);
        }

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            klient.ilosc_produktow = 300;
            msgsnd(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long int), 0);
        }

        cout << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[0] << endl;
        cout << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[1] << endl;
        cout << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[2] << endl;
  
    }

    cout << "Koniec kasy " << endl;
    exit(0);
}
