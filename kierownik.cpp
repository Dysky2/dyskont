#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kierownik_ma_dzialac = 1; 

void zamknij_kierownika(int) {
    czy_kierownik_ma_dzialac = 0;
}
void kontynuj_prace(int) {}

int main(int argc, char * argv[]) {

    if (argc < 5) {
        perror("Uzyto za malo argumentow w kierowniku");
        exit(EXIT_FAILURE);
    }

    utworz_grupe_semaforowa();

    komunikat << "[Kierownik] wchodzi do dyskontu\n";

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
    int sem_id = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);

    if(stan_dyskontu == (void*) -1) {
        perror("Bledne podlaczenie pamieci dzielonej w kierowniku");
        exit(EXIT_FAILURE);
    }

    stan_dyskontu->pid_kierownika = getpid();

    int show_panel = 1;

    while (czy_kierownik_ma_dzialac) {
        int option = 0;
        if(show_panel) {
            komunikat << "Panel kierownika, opcje do wybrania: \n";
            komunikat << "1.Otwarcie Kasy 2\n";
            komunikat << "2.Zamknij kase (1 lub 2)\n";
            komunikat << "3.Koniec dyskontu\n";
        }

        alarm(10);
        if(cin >> option) {
            alarm(0);
            show_panel = 1;
        } else {
            alarm(0);
            show_panel = 0;
            if(czy_kierownik_ma_dzialac == 0) {
                break;
            }
            cin.clear();
            cin.ignore(1000, '\n');
            continue;
        }

        int kasa = 0;
        switch (option) {
            case 1:
                if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] == 0) {
                    operacja_v(sem_id, SEMAFOR_ILOSC_KAS);
                    komunikat << "Zarzadam otwarcie kasy stacjonarnej 2\n";
                    kill(stan_dyskontu->pid_kasy[ID_KASY_STACJONARNEJ_2], SIGUSR1);
                } else {
                    komunikat << "Ta kasa jest juz otwarta\n";
                }
                break;
            case 2:
                komunikat << "Wybierz kase 1 lub 2: \n";
                if (!(cin >> kasa)) {
                    cin.clear();
                    cin.ignore(1000, '\n');
                    komunikat << "Musisz podac liczbe!\n";
                    break; 
                }
                if(kasa == 1) {
                    if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] == 1) {

                        komunikat << "Zarzadzam zamkniecie kasy 1\n";
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 2;

                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac1) > 0 || stan_dyskontu->dlugosc_kolejki[1] > 0) {
                            komunikat << "Nie moge zamknac kasy poniewaz, ludzie stoja dalej w kolejce\n";
                            sleep(2);
                        }

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 0;
                        break;
                    } else {
                        komunikat << "Kasa jest juz zamknieta\n";
                    }
                } else if(kasa == 2) {
                    if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] == 1) {
                        
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 2;
                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac2) > 0  || stan_dyskontu->dlugosc_kolejki[2] > 0) {
                            komunikat << "Nie moge zamknac kasy poniewaz, ludzie stoja dalej w kolejce\n";
                            sleep(2);
                        }

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 0;
                        break;
                    }else {
                        komunikat << "Kasa jest juz zamknieta\n";
                    }
                } else {
                    komunikat << "Podano niepoprawny komunikat\n";
                }
                break;
            case 3:
                komunikat << "Kierownik zarzadzil zamkniecie sklepu\n";
                kill(stan_dyskontu->pid_dyskontu, SIGTERM);
                czy_kierownik_ma_dzialac = 0;
                break;
            case 4:
                komunikat << "Kierownik opusza dyskont\n";
                exit(0);
                break;
            default:
                komunikat << "Wywolano zla opcje\n";
                cin.clear();
                break;
        }
    }

    komunikat << "Kierownik opusza dyskont \n";
    if(shmdt(stan_dyskontu) == -1) {
        perror("Bledne odlaczenie pamieci");
        exit(EXIT_FAILURE);
    }
    exit(0);
}