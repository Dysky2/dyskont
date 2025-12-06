#include <iostream>
#include <unistd.h>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    cout << "Tutaj kasa witam" << endl;

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);

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
        checkError( msgrcv(msqid_kolejka_samo, &klient, sizeof(klient) - sizeof(long), -2, 0), "Blad odebrania wiadomosci" );

        if (klient.ilosc_produktow == -1 || klient.klient_id == getpid()) {
            break;
        }
        lista_kas->liczba_ludzi[0]--;

        cout << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Z TEJ STRONY: " << getpid() << endl;
        sleep(5);
        cout << "Klient wychodzi ze sklepu" << endl;

        cout << "WARTOSC SEMAFORA W KASIE - 1: " << semctl(semid_klienci, 0, GETVAL) << endl;       
        struct sembuf operacjaP = {0, -1, 0};
        checkError( semop(semid_klienci, &operacjaP, 1 ), "Blad obnizenia semafora" );
        cout << "WARTOSC SEMAFORA W KASIE - 2: " << semctl(semid_klienci, 0, GETVAL) << endl;   
    }

    cout << "Koniec kasy " << endl;
    exit(0);
}
