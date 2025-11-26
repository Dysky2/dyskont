#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <unistd.h>

#include "utils.h"

using namespace std;

int main() {
    srand(time(0));

    const double simulation_speed = 1.0;
    const int simulation_time = 30;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    cout << "---------------------------------------" << endl;
    cout << "---    Dyskont otwraty zapraszamy   ---" << endl;
    cout << "---------------------------------------" << endl << endl;

	key_t key = ftok(".", 'A');
    int semid = checkError( semget(key, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    while(chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(4 / simulation_speed));

        cout << "WARTOSC SEMAFORA: " << semctl(semid, 0, GETVAL) << endl;

        int id = checkError( fork() , "Blad forka");
        if(id==0) {
            struct sembuf operacjaP = {0, 1, SEM_UNDO};
            checkError( semop(semid, &operacjaP, 1), "Blad podniesienia semafora" );
            checkError( execl("./klient", "klient", to_string(semid).c_str(), NULL), "Blad exec" );
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

    cout << "Czas wykonania: " << elapsed.count() << endl;

    return 0;
}
