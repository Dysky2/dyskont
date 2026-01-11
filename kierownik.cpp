#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_kierownik_ma_dzialac = 1; 

void zamknij_kierownika(int) {
    czy_kierownik_ma_dzialac = 0;
    close(STDIN_FILENO);
}

int main(int argc, char * argv[]) {
    srand(time(0) + getpid());

    prctl(PR_SET_PDEATHSIG, SIGTERM);
    
    if (argc < 5) {
        showError("Uzyto za malo argumentow w kierowniku");
        exit(EXIT_FAILURE);
    }

    utworz_grupe_semaforowa();

    signal(SIGTERM, zamknij_kierownika);
    signal(SIGINT, zamknij_kierownika);

    int shmid_kasy = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msqid_kolejka_stac1 = atoi(argv[3]);
    int msqid_kolejka_stac2 = atoi(argv[4]);
    string nazwa_katalogu = argv[5];

    ustaw_nazwe_katalogu(nazwa_katalogu);

    komunikat << "[Kierownik] wchodzi do dyskontu\n";

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);

    if(stan_dyskontu == (void*) -1) {
        showError("[Kierownik] Bledne podlaczenie pamieci dzielonej w kierowniku");
        exit(EXIT_FAILURE);
    }

    operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
    stan_dyskontu->pid_kierownika = getpid();
    operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

    int show_panel = 1;

    while (czy_kierownik_ma_dzialac) {
        int option = 0;
        if(show_panel) {
            komunikat << "[Kierownik] Panel kierownika, opcje do wybrania: \n";
            komunikat << "[Kierownik] 1.Otwarcie Kasy 2\n";
            komunikat << "[Kierownik] 2.Zamknij kase (1 lub 2)\n";
            komunikat << "[Kierownik] 3.Koniec dyskontu\n";
        }

        if (!(cin >> option)) {
            if(!czy_kierownik_ma_dzialac) break;
            cin.clear();
            cin.ignore(1000, '\n');
            komunikat << "[Kierownik] Musisz podac liczbe!\n";
            continue;
        }

        int kasa = 0;
        switch (option) {
            case 1: {
                operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                int status_stac2 = stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2];
                operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                if(status_stac2 == 0) {
                    operacja_v(sem_id, SEMAFOR_ILOSC_KAS);

                    komunikat << "[Kierownik] Zarzadam otwarcie kasy stacjonarnej 2\n";

                    operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                    stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 1;
                    operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                    operacja_v(sem_id, SEMAFOR_DZIALANIE_KASY_STAC2);
                    
                } else {
                    komunikat << "[Kierownik] Ta kasa jest juz otwarta\n";
                }
                break;
            }
            case 2:
                komunikat << "[Kierownik] Wybierz kase 1 lub 2: \n";
                if (!(cin >> kasa)) {
                    if(!czy_kierownik_ma_dzialac) break;
                    cin.clear();
                    cin.ignore(1000, '\n');
                    komunikat << "[Kierownik] Musisz podac liczbe!\n";
                    break; 
                }

                if(kasa == 1) {
                    operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                    int status_kasy = stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1];
                    operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);  

                    if(status_kasy == 1) {
                        komunikat << "[Kierownik] Zarzadzam zamkniecie kasy 1\n";

                        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 2;
                        int dlugosc_kolejki = stan_dyskontu->dlugosc_kolejki[1];
                        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);

                        bool komunikat_wyswietlony = false;
                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac1) > 0 || dlugosc_kolejki > 0) {
                            if (!komunikat_wyswietlony) {
                                komunikat << "[Kierownik] Czekam na obsluzenie klientow (Kasa 1)...\n";
                                komunikat_wyswietlony = true;
                            }

                            usleep(3000000);

                            operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                            dlugosc_kolejki = stan_dyskontu->dlugosc_kolejki[1];
                            operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);
                        }

                        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 0;
                        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                        komunikat << "[Kierownik] Kasa 1 pomyslnie zamknieta\n";
                        break;
                    } else {
                        komunikat << "[Kierownik] Kasa jest juz zamknieta\n";
                    }
                } else if(kasa == 2) {
                    operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                    int status_kasy = stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2];
                    operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                    if(status_kasy == 1) {
                        komunikat << "[Kierownik] Zarzadzam zamkniecie kasy 2\n";

                        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 2;
                        int dlugosc_kolejki = stan_dyskontu->dlugosc_kolejki[2];
                        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                        operacja_p(sem_id, SEMAFOR_ILOSC_KAS);

                        bool komunikat_wyswietlony = false;
                        while (pobierz_ilosc_wiadomosci(msqid_kolejka_stac2) > 0  || dlugosc_kolejki > 0) {

                            if (!komunikat_wyswietlony) {
                                komunikat << "[Kierownik] Czekam na obsluzenie klientow (Kasa 2)...\n";
                                komunikat_wyswietlony = true;
                            }

                            usleep(3000000);

                            operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                            dlugosc_kolejki = stan_dyskontu->dlugosc_kolejki[2];
                            operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);
                        }

                        operacja_p(sem_id, SEMAFOR_STAN_DYSKONTU);
                        stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] = 0;
                        operacja_v(sem_id, SEMAFOR_STAN_DYSKONTU);

                        komunikat << "[Kierownik] Kasa 2 pomyslnie zamknieta\n";   
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