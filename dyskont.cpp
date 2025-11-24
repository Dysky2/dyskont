#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int randomTime(int time) {
    return rand() % time; 
}

int main() {
    srand(time(0));

    const double simulation_speed = 1.0;
    const int simulation_time = 60;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    cout << "---------------------------------------" << endl;
    cout << "---    Dyskont otwraty zapraszamy   ---" << endl;
    cout << "---------------------------------------" << endl << endl;

    while(chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(8 / simulation_speed));

        int id=fork();
        if(id==-1) {
            cerr << "Fork nie powiodl sie!" << endl;
            exit(1);
        }
        if(id==0) {
            execl("./klient", "klient", NULL);
            perror("execl failed");
            exit(0);
        }

    }

    auto end = chrono::steady_clock::now();

    chrono::duration<double, milli> elapsed = end - start;

    cout << "Czas wykonania: " << elapsed.count() << endl;

    return 0;
}