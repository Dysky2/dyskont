#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kierownik_ma_dzialac = 1; 

void zamknij_kierownika(int) {
    czy_kierownik_ma_dzialac = 0;
    close(STDIN_FILENO);
}
void kontynuj_prace(int) {}

int main(int argc, char * argv[]) {

    if (argc < 5) {
        perror("Uzyto za malo argumentow w kierowniku");
        exit(EXIT_FAILURE);
    }

    utworz_grupe_semaforowa();

    signal(SIGTERM, zamknij_kierownika);

    signal(SIGALRM, kontynuj_prace);

    int shmid_kasy = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);
    string nazwa_katalogu = argv[5];

    ustaw_nazwe_katalogu(nazwa_katalogu);

    komunikat << "[Kierownik] wchodzi do dyskontu\n";

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);

    if(stan_dyskontu == (void*) -1) {
        perror("[Kierownik] Bledne podlaczenie pamieci dzielonej w kierowniku");
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

        if (!(cin >> option)) {
            cin.clear();
            cin.ignore(1000, '\n');
            komunikat << "[Kierownik] Musisz podac liczbe!\n";
            break; 
        }

        int kasa = 0;
        switch (option) {
            case 1:
                if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] == 0) {
                    operacja_v(sem_id, SEMAFOR_ILOSC_KAS);
                    komunikat << "[Kierownik] Zarzadam otwarcie kasy stacjonarnej 2\n";
                    kill(stan_dyskontu->pid_kasy[ID_KASY_STACJONARNEJ_2], SIGUSR1);
                } else {
                    komunikat << "[Kierownik] Ta kasa jest juz otwarta\n";
                }
                break;
            case 2:
                komunikat << "[Kierownik] Wybierz kase 1 lub 2: \n";
                if (!(cin >> kasa)) {
                    cin.clear();
                    cin.ignore(1000, '\n');
                    komunikat << "[Kierownik] Musisz podac liczbe!\n";
                    break; 
                }
                if(kasa == 1) {
                    if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] == 1) {

                        komunikat << "[Kierownik] Zarzadzam zamkniecie kasy 1\n";
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 2;

                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac1) > 0 || stan_dyskontu->dlugosc_kolejki[1] > 0) {
                            komunikat << "[Kierownik] Nie moge zamknac kasy poniewaz, ludzie stoja dalej w kolejce\n";
                            sleep(2);
                        }

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 0;
                        break;
                    } else {
                        komunikat << "[Kierownik] Kasa jest juz zamknieta\n";
                    }
                } else if(kasa == 2) {
                    if(stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] == 1) {
                        
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 2;
                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac2) > 0  || stan_dyskontu->dlugosc_kolejki[2] > 0) {
                            komunikat << "[Kierownik] Nie moge zamknac kasy poniewaz, ludzie stoja dalej w kolejce\n";
                            sleep(2);
                        }

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 0;
                        break;
                    }else {
                        komunikat << "[Kierownik] Kasa jest juz zamknieta\n";
                    }
                } else {
                    komunikat << "[Kierownik] Podano niepoprawny komunikat\n";
                }
                break;
            case 3:
                komunikat << "[Kierownik] Kierownik zarzadzil zamkniecie sklepu\n";
                kill(stan_dyskontu->pid_dyskontu, SIGTERM);
                czy_kierownik_ma_dzialac = 0;
                break;
            case 4:
                czy_kierownik_ma_dzialac = 0;
                break;
            default:
                komunikat << "[Kierownik] Wywolano zla opcje\n";
                cin.clear();
                cin.ignore(1000, '\n');
                break;
        }
    }

    komunikat << "[Kierownik] opusza dyskont \n";
    checkError( shmdt(stan_dyskontu), "Blad odlaczenia pamieci stanu" );
    exit(0);
}