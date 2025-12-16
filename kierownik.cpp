#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    komunikat << "Kierownik wchodzi do dyskontu\n";

    int shmid_kasy = atoi(argv[1]);
    
    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    while (1) {
        int option;
        komunikat << "Panel kierownika, opcje do wybrania: \n";
        komunikat << "1. Otwarcie Kasy 2\n2.Zamknij kase (1 lub 2)\n3.Koniec dyskontu\n";
        cin >> option;
        switch (option) {
        case 1:
            kill(lista_kas->pid_kasy[7], SIGUSR1);
            break;
        case 2:
            
            break;

        case 3:

            break;
        case 4:
            exit(0);
            break;
        default:
            komunikat << "Wywolano zla opcje\n";
            break;
        }
        // pause();
    }

    komunikat << "Kierownik opusza dyskont\n";
    exit(0);
}