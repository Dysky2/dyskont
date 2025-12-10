#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    komunikat << "Otwieram kase stacjonarna " << getpid() << "\n";

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);
    int semid_kolejki = atoi(argv[5]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    while(1) {
        Klient klient;

        alarm(30);  

        msgrcv(msqid_kolejka_stac1, &klient, sizeof(klient) - sizeof(long), 1, 0);

        alarm(0);
    
        if(klient.ilosc_produktow == -1 || klient.klient_id==getpid() || errno == EINTR) {
            break;
        }
        sleep(randomTime(10));
        
        komunikat << "ODEBRANO KOMUNIKAT W KASJERZE " << klient.klient_id << " Z TEJ STRONY: " << getpid() << " nrkasy to " << klient.nrKasy << "\n";
        
        if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
            komunikat << "KASJER probuje odjac z kolejki gdzie jest 0" << "\n" << "\n";
        } else {
            zmien_wartosc_kolejki(semid_kolejki, lista_kas, klient.nrKasy , -1);
        }

        if(kill(klient.klient_id, 0) == 0) {
            klient.mtype = klient.klient_id;
            msgsnd(msqid_kolejka_stac1, &klient, sizeof(klient) - sizeof(long int), 0);
        }

        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[0] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[1] << "\n";
        komunikat << "Ilosc ludzi w kolejce OD KASJERA " << lista_kas->liczba_ludzi[2] << "\n";
    }

    komunikat << "Zamykam kase stacjonarna: " << getpid() << "\n" << "\n";
    exit(0);
}