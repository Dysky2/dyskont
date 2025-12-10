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

        msgrcv(msqid_kolejka_stac1, &klient, sizeof(klient) - sizeof(long), 0, 0);

        alarm(0);
    
        if(klient.ilosc_produktow == -1 || klient.klient_id==getpid() || errno == EINTR) {
            break;
        }

        cout << "ODEBRANO KOMUNIKAT W KASJERZE " << klient.klient_id << " Z TEJ STRONY: " << getpid() << " nrkasy to " << klient.nrKasy << endl;
        sleep(randomTime(15));
        cout << "Klient OD KASJERA wychodzi ze sklepu" << endl;

        if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
            cout << "KASJER probuje odjac z kolejki gdzie jest 0" << endl << endl;
        } else {
            zmien_wartosc_kolejki(semid_kolejki, lista_kas, klient.nrKasy , -1);
        }

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            klient.ilosc_produktow = 300;
            msgsnd(msqid_kolejka_stac1, &klient, sizeof(klient) - sizeof(long int), 0);
        }

    }

    cout << "Zamykam kase stacjonarna: " << getpid() << endl << endl;
    exit(0);
}