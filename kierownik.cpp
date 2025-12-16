#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kierownik_ma_dzialac = 1; 

void zamknij_kierownika(int sig) {
    czy_kierownik_ma_dzialac = 0;
}
void kontynuj_prace(int sig) {}

int main(int argc, char * argv[]) {
    komunikat << "Kierownik wchodzi do dyskontu\n";

    struct sigaction sa_term;
    memset(&sa_term, 0, sizeof(struct sigaction));
    sa_term.sa_handler = zamknij_kierownika;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);

    struct sigaction sa_alarm;
    memset(&sa_alarm, 0, sizeof(struct sigaction));
    sa_alarm.sa_handler = kontynuj_prace;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alarm, NULL);

    int shmid_kasy = atoi(argv[1]);
    
    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);
    lista_kas->pid_kierownika = getpid();

    int show_panel = 1;

    while (czy_kierownik_ma_dzialac) {
        int option = 0;
        if(show_panel) {
            komunikat << "Panel kierownika, opcje do wybrania: \n";
            komunikat << "1. Otwarcie Kasy 2\n2.Zamknij kase (1 lub 2)\n3.Koniec dyskontu\n";
        }

        alarm(10);

        if(cin >> option) {
            alarm(0);
            show_panel = 1;
        } else {
            show_panel = 0;
            if(czy_kierownik_ma_dzialac == 0) {
                break;
            }

            cin.clear();
            continue;
        }

        switch (option) {
        case 1:
            if(lista_kas->status[7] == 0) {
                kill(lista_kas->pid_kasy[7], SIGUSR1);
            } else {
                komunikat << "Ta kasa jest juz otwarta\n";
            }
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
    }

    komunikat << "Kierownik opusza dyskont\n";
    exit(0);
}