#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

using namespace std;

int randomTime(int time) {
    return rand() % time; 
}

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
	int semid = semget(key, 1, IPC_CREAT|0600);

    while(chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(4 / simulation_speed));

        cout << "WARTOSC SEMAFORA: " << semctl(semid, 0, GETVAL) << endl;

        int id=fork();
        if(id==-1) {
            cerr << "Fork nie powiodl sie!" << endl;
            exit(1);
        }
        if(id==0) {
            struct sembuf operacjaP = {0, 1, SEM_UNDO};
            semop(semid, &operacjaP, 1);
            execl("./klient", "klient", to_string(semid).c_str(), NULL);
            perror("execl failed");
            exit(0);
        }

    }

    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    auto end = chrono::steady_clock::now();

    chrono::duration<double, milli> elapsed = end - start;

    semctl(semid, 0, IPC_RMID);

    cout << "Czas wykonania: " << elapsed.count() << endl;

    return 0;
}
