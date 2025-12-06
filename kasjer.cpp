#include <iostream>

#include "utils.h"

using namespace std;


int main(int argc, char * argv[]) {
    cout << "Otwieram kase stacjonarna " << getpid() << endl << endl;

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);
    int semid_kolejki = atoi(argv[5]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    while(1) {
        klientWzor klient;

        alarm(30);

        msgrcv(msqid_kolejka_stac1, &klient, sizeof(klient)- sizeof(long), 0, 0);

        alarm(0);

        if(klient.ilosc_produktow == -1 || klient.klient_id==getpid()) {
            break;
        }

        cout << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Z TEJ STRONY: " << getpid() << endl;
        sleep(randomTime(4));
        cout << "Klient OD KASJERA wychodzi ze sklepu" << endl;

        zmien_wartosc_kolejki(semid_kolejki, lista_kas, klient.nrKasy , -1);

        cout << "WARTOSC SEMAFORA W KASIE - 1: " << semctl(semid_klienci, 0, GETVAL) << endl;       
        struct sembuf operacjaP = {0, -1, 0};
        checkError( semop(semid_klienci, &operacjaP, 1 ), "Blad obnizenia semafora" );
        cout << "WARTOSC SEMAFORA W KASIE - 2: " << semctl(semid_klienci, 0, GETVAL) << endl; 

    }

    cout << "Zamykam kase stacjonarna: " << getpid() << endl << endl;
    exit(0);
}