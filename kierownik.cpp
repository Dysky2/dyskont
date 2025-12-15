#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    komunikat << "Kierownik wchodzi do dyskontu\n";

    int shmid_kasy = atoi(argv[1]);
    
    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    while (1) {
       pause();
       break;
    }
    
    // kill(lista_kas->pid_kasy[7], SIGUSR1);

    komunikat << "Kierownik opusza dyskont\n";
    exit(0);
}