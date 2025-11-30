#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "utils.h"

using namespace std;

int main() {
    srand(time(0));

    const double simulation_speed = 1.0;
    const int simulation_time = 30;
    const int startowa_ilosc_kas = 3;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    cout << "---------------------------------------" << endl;
    cout << "---    Dyskont otwraty zapraszamy   ---" << endl;
    cout << "---------------------------------------" << endl << endl;

    // Ilczba klientow w sklepie
	key_t key = ftok(".", 'A');
    int semid = checkError( semget(key, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    // Pamiec dzielona dla kas
    key_t key_kasy = ftok(".", 'B');
    int shmId_kasy = checkError( shmget(key_kasy, sizeof(kasy), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    kasy * lista_kas = (kasy *) shmat(shmId_kasy, NULL, 0);

    // kolejka do kas
    key_t key_kolejka = ftok(".", 'C');
    int msqid_kolejka = checkError( msgget(key_kolejka, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    cout << "Otwieram kasy samoobsługowe" << endl;
    for(int i=0;i<3;i++) {
        int pid = checkError( fork(), "Blad utowrzenia forka");
        if ( pid == 0 ) {
            checkError( execl("./kasa", "kasa", to_string(shmId_kasy).c_str(), to_string(msqid_kolejka).c_str(), to_string(semid).c_str(), NULL) ,"Blad wywolania execa");
        } else {
            if( i < startowa_ilosc_kas) {
                lista_kas->pid_kasy[i] = pid;
                lista_kas->status[i]= 1;
            }
        }
    }

    while( chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(2 / simulation_speed));

        int id = checkError( fork() , "Blad forka");
        if(id==0) {
            struct sembuf operacjaP = {0, 1, SEM_UNDO};
            checkError( semop(semid, &operacjaP, 1), "Blad podniesienia semafora" );
            cout << "WARTOSC SEMAFORA: " << semctl(semid, 0, GETVAL) << endl;
            checkError( execl("./klient", "klient", to_string(semid).c_str(), to_string(msqid_kolejka).c_str(), NULL), "Blad exec" );
        } 
    }


    // Wyslanie wiadomosci o zamkniecie kas
    for(int i=0; i < 8;i++) {
        if(lista_kas->status[i] == 1) {
            klientWzor klient;
            klient.mtype=1;
            klient.klient_id=10;
            klient.ilosc_produktow=-1;

            msgsnd(msqid_kolejka, &klient, sizeof(klient)- sizeof(long), 0);
        }
    }


    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    if (errno != ECHILD) {
        checkError(-1, "Blad czekania na klientwo");
    }

    auto end = chrono::steady_clock::now();

    chrono::duration<double, milli> elapsed = end - start;

    checkError(semctl(semid, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError(msgctl(msqid_kolejka, IPC_RMID, NULL), "Blad zamknieca kolejki komunikatow");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");

    cout << "Czas wykonania: " << elapsed.count() << endl;

    return 0;
}
