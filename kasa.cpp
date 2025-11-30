#include <iostream>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    cout << "Tutaj kasa witam" << endl;

    kasy * lista_kas = (kasy *) shmat(atoi(argv[1]), NULL , 0);
    struct sembuf operacjaP = {0, -1, SEM_UNDO}; 

    for(int i=0;i<8;i++) {
        cout << lista_kas->pid_kasy[i] << " ";
    }
    for(int i=0;i<8;i++) {
        cout << lista_kas->status[i] << " ";
    }

    cout << endl;

    
    while(1) {
        klientWzor klient;
        checkError( msgrcv(atoi(argv[2]), &klient, sizeof(klient) - sizeof(long), -2, 0), "Blad odebrania wiadomosci" );

        if(klient.ilosc_produktow == -1) {
            break;
        }

        cout << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Z TEJ STRONY: " << getpid() << endl;
        sleep(3);
        cout << "Klient wychodzi ze sklepu" << endl;
        checkError(semop(stoi(argv[3]), &operacjaP, 1), "Blad obnizenia semafora" );
    }


    cout << "Koniec kasy " << endl;
    exit(0);
}
