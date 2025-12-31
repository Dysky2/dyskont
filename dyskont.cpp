#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t ewakuacja = 0; 

int shmId_kasy = 0, dyskont_sem_id = 0, obsluga_id = 0, shm_id_klienci=0;

void usun_zombie(int) {

    int saved_ernno = errno;

    while( waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_ernno;
}

void wylacz_sklep(int) {
    ewakuacja = 1;
}

int main() {
    srand(time(0) + getpid());

    if(signal(SIGINT, wylacz_sklep) == SIG_ERR) {
        showError("[DYSKONT] Nie udalo sie ustawic handlera SIGINT");
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, usun_zombie) == SIG_ERR) {
        showError("[DYSKONT] Nie udalo sie ustawic handlera SIGCHLD");
        exit(EXIT_FAILURE);
    }

    if(signal(SIGTERM, wylacz_sklep) == SIG_ERR) {
        showError("[DYSKONT] Nie udalo sie ustawic handlera SIGTERM");
        exit(EXIT_FAILURE);
    }

    dyskont_sem_id = utworz_grupe_semaforowa();
    ustaw_poczatkowe_wartosci_semaforow(dyskont_sem_id);
    
    // Tworze pamiec dzielona dla listy klientow w dyskoncie
    shm_id_klienci = checkError( shmget(utworz_klucz('B'), sizeof(DaneListyKlientow), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej dla klientow" );

    DaneListyKlientow * dane_klientow = (DaneListyKlientow *) shmat(shm_id_klienci, NULL ,0);

    if(dane_klientow == (void*) -1) {
        showError("[DYSKONT] Bledne podlaczenie pamieci dzielonej klientow w dyskoncie");
        exit(EXIT_FAILURE);
    }
    
    // Tworze pamiec dzielona dla stanu calego dyskontu
    shmId_kasy = checkError( shmget(utworz_klucz('C'), sizeof(StanDyskontu), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmId_kasy, NULL, 0);

    if(stan_dyskontu == (void*) -1) {
        showError("[DYSKONT] Bledne podlaczenie pamieci dzielonej stanu dyskontu");
        exit(EXIT_FAILURE);
    }

    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
    stan_dyskontu->sredni_czas_obslugi[0] = CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA;
    stan_dyskontu->sredni_czas_obslugi[1] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;
    stan_dyskontu->sredni_czas_obslugi[2] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;
    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

    stan_dyskontu->pid_dyskontu = getpid();
    for(int i=0; i<8; i++) {
        if(i < 3) {
            stan_dyskontu->dlugosc_kolejki[i] = 0;
        }
        stan_dyskontu->pid_kasy[i] = 0;
        stan_dyskontu->status_kasy[i] = 0;
    }

    // kolejka do kas samoobsługowych
    int msqid_kolejka_samo = checkError( msgget(utworz_klucz('D'), IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kasy stacjonarnej_1
    int msqid_kolejka_stac1 = checkError( msgget(utworz_klucz('E'), IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kas stacjonarnej_2
    int msqid_kolejka_stac2 = checkError( msgget(utworz_klucz('F'), IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do obsługi 
    int msqid_kolejka_obsluga = checkError( msgget(utworz_klucz('G'), IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    int ilosc_otwratych_kas = 0;

    time_t dyskont_start = time(NULL);
    time_t dyskont_koniec = dyskont_start + ((int) (simulation_time / simulation_speed));

    string nazwa_katalogu = utworz_katalog_na_logi();

    komunikat << "[Dyskont] Pid dyskontu: " << getpid() << "\n";
    komunikat << "[Dyskont] ---------------------------------------" << "\n";
    komunikat << "[Dyskont] ---    Dyskont otwraty zapraszamy   ---" << "\n";
    komunikat << "[Dyskont] ---------------------------------------" << "\n";

    int kierownik_pid = checkError(fork(), "Blad utowrzenia forka");
    if (kierownik_pid == 0) {
        string komenda_bash = "./kierownik " + 
                            to_string(shmId_kasy) + " " + 
                            to_string(dyskont_sem_id) + " " + 
                            to_string(msqid_kolejka_stac1) + " " + 
                            to_string(msqid_kolejka_stac2) + " " +
                            nazwa_katalogu + "; exec bash";
        execlp("tmux",
            "tmux",
            "split-window",
            "-h",
            "-c", ".",
            komenda_bash.c_str(),
            NULL
        );

        showError("[Dyskont] Blad uruchomienia nowego okna");
        exit(EXIT_FAILURE);
    }

    komunikat << "[DYSKONT] Obsluga wchodzi do sklepu" << "\n";
    int opid = checkError(fork(), "Blad utowrzenia forka");
    if (opid == 0) {
        checkError(
            execl(
                "./obsluga", 
                "obsluga",
                to_string(msqid_kolejka_obsluga).c_str(),
                nazwa_katalogu.c_str(),
                NULL
            ),
            "Blad wywolania execa"
        );
    }
    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
    obsluga_id = opid;
    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

    // Tworze wszytkie kasy i zostawiam otwarte tylko 3 samoobslugowe
    for (int i=0;i<8;i++) {
        if(i < 6) {
            int pid = checkError( fork(), "Blad utowrzenia forka");
            if ( pid == 0 ) {
                checkError( 
                    execl(
                        "./kasa", "kasa", 
                        to_string(dyskont_sem_id).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_samo).c_str(),
                        to_string(msqid_kolejka_obsluga).c_str(),
                        nazwa_katalogu.c_str(),
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                if( i < startowa_ilosc_kas) {
                    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                    stan_dyskontu->pid_kasy[i] = pid;
                    stan_dyskontu->status_kasy[i]= 1;
                    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                    ilosc_otwratych_kas++;
                } else {
                    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                    stan_dyskontu->pid_kasy[i] = pid;
                    stan_dyskontu->status_kasy[i]= 0;
                    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                }
            }
        } else {
            int pid = checkError( fork(), "Blad utowrzenia forka");
            if ( pid == 0 ) {
                checkError( 
                    execl(
                        "./kasjer", "kasjer", 
                        to_string(dyskont_sem_id).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_stac1).c_str(), 
                        to_string(msqid_kolejka_stac2).c_str(),
                        nazwa_katalogu.c_str(),
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                stan_dyskontu->pid_kasy[i] = pid;
                stan_dyskontu->status_kasy[i] = 0;
                operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
            }
        }
    }
    sleep(1);

    ListaKlientow * lista_klientow = new ListaKlientow(dane_klientow, 1);
    while( time(NULL) < dyskont_koniec && !ewakuacja) {

        while(dane_klientow->ilosc > MAX_ILOSC_KLIENTOW - 2 && dane_klientow->ilosc < MAX_ILOSC_KLIENTOW) {
            if(ewakuacja) break;
            sleep(1);
        }

        sleep(randomTime(3 / simulation_speed));

        if (time(NULL) >= dyskont_koniec || ewakuacja) {
            break;
        }

        int id = checkError( fork() , "Blad forka");
        if(id==0) {
            checkError(
                execl("./klient", "klient", 
                    to_string(dyskont_sem_id).c_str(),
                    to_string(shmId_kasy).c_str(),
                    to_string(msqid_kolejka_samo).c_str(),
                    to_string(msqid_kolejka_stac1).c_str(),
                    to_string(msqid_kolejka_stac2).c_str(),
                    to_string(shm_id_klienci).c_str(),
                    nazwa_katalogu.c_str(),
                    NULL
                ), 
                "Blad exec" 
            );
        } else {
            operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
            lista_klientow->dodaj_klienta_do_listy(id);
            operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
        }

        operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
        int dlugosc_koleki = stan_dyskontu->dlugosc_kolejki[ID_KASY_SAMOOBSLUGOWEJ];
        int status_stac_1 = stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1];
        operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

        // otwarcie kasy stacjonarnej 1
        if(dlugosc_koleki > 3 && status_stac_1 == 0) {

            operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
            stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] = 1;
            operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

            operacja_v(dyskont_sem_id, SEMAFOR_ILOSC_KAS);

            kill(stan_dyskontu->pid_kasy[ID_KASY_STACJONARNEJ_1], SIGUSR2);
        }

        operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
        int ilosc_klientow = semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL);
        operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);

        // Zamknięcie kasy samoobsługowej w zależności od ilości klientów
        if(ilosc_klientow < LICZBA_KLIENTOW_NA_KASE * (ilosc_otwratych_kas - 3)) {
            for(int i = 5;i > 2;i--) {
                if(stan_dyskontu->status_kasy[i] == 1) {
                    komunikat << "[DYSKONT] ZAMYKAM KASE " << i + 1 << "\n";

                    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                    stan_dyskontu->status_kasy[i] = 0;
                    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

                    kill(stan_dyskontu->pid_kasy[i], SIGUSR2);
                    operacja_p(dyskont_sem_id, SEMAFOR_ILOSC_KAS);
                    ilosc_otwratych_kas--;
                    break;
                }
            }
        }

        // Otwarcie kasy samoobsługowej w zależności od ilości klientów
        if (ilosc_klientow >= ilosc_otwratych_kas * LICZBA_KLIENTOW_NA_KASE && ilosc_otwratych_kas <= 5) {
            int wolny_status = -1;
            for(int i = 0;i < 6;i++) {
                operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                int status_kasy = stan_dyskontu->status_kasy[i];
                operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

                if (status_kasy == 0) {
                    wolny_status = i;
                    break;
                }
            }
            if(wolny_status != -1) {
                komunikat << "[DYSKONT] OTWIERAM KASE " << wolny_status + 1 << "\n";
                
                operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                stan_dyskontu->status_kasy[wolny_status] = 1;
                operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                
                ilosc_otwratych_kas++;

                operacja_v(dyskont_sem_id, SEMAFOR_ILOSC_KAS);
                kill(stan_dyskontu->pid_kasy[wolny_status], SIGUSR1);
            }
        }

        stringstream semafory;
        semafory << "[DYSKONT] Semafory: ";
        operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
        for(int i=0;i<ILOSC_SEMAFOROW;i++) {
            semafory << semctl(dyskont_sem_id, i, GETVAL) << " ";
        }
        operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
        komunikat << semafory.str() << "\n";
        semafory.clear();

        stringstream kolejki;
        kolejki << "[DYSKONT] Ilosc ludzi w kolejce ";
        operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
        for(int i=0; i < ILOSC_KOLJEJEK; i++) {
            kolejki << stan_dyskontu->dlugosc_kolejki[i] << " ";
        }
        operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);   
        komunikat << kolejki.str() << "\n";
        kolejki.clear();
    }

    if(ewakuacja) {
        komunikat << "[DYSKONT] Kierownik zarzadzil zamkniecie dyskontu\n";
    }

    operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
    int ilosc_klientow = semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL);
    operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);

    // czekanie az klienci opuszcza sklep
    while (ilosc_klientow > 0) {

        operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
        ilosc_klientow = semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL);
        operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);

        // Otwarcie kasy samoobsługowej w zależności od ilości klientów
        if(ilosc_klientow < LICZBA_KLIENTOW_NA_KASE * (ilosc_otwratych_kas - 3)) {
            for(int i = 5;i > 2;i--) {
                if(stan_dyskontu->status_kasy[i] == 1) {
                    komunikat << "[DYSKONT] ZAMYKAM KASE " << i + 1 << "\n";
                    operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                    stan_dyskontu->status_kasy[i] = 0;
                    operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);

                    kill(stan_dyskontu->pid_kasy[i], SIGUSR2);
                    operacja_p(dyskont_sem_id, SEMAFOR_ILOSC_KAS);
                    ilosc_otwratych_kas--;
                    break;
                }
            }
        }

        if(ewakuacja) {
            for(int i=0;i<8;i++) {
                operacja_p(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
                if(stan_dyskontu->status_kasy[i] != 0) {
                    stan_dyskontu->status_kasy[i] = 0;
                    kill(stan_dyskontu->pid_kasy[i], SIGUSR2);
                }
                operacja_v(dyskont_sem_id, SEMAFOR_STAN_DYSKONTU);
            }
            
            int kopia_pidow[MAX_ILOSC_KLIENTOW];
            int kopia_ilosc = 0;

            operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
            kopia_ilosc = dane_klientow->ilosc;
            for(int i=0; i < kopia_ilosc; i++) {
                kopia_pidow[i] = dane_klientow->lista_klientow[i];
            }
            operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);

            for(int i=0; i < kopia_ilosc; i++) {
                if(kopia_pidow[i] > 0) {
                    kill(kopia_pidow[i], SIGTERM);
                }
                usleep(1000);
            }
        }

        sleep(2);
        
        operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
        komunikat << "[DYSKONT] Zostalo: " << semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << " klientow w dyskoncie" << "\n";
        ilosc_klientow = semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL);
        operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
    }

    // Kierownik opusza dyskont
    if(stan_dyskontu->pid_kierownika > 0) {
        int kill_status_kierownik = kill(stan_dyskontu->pid_kierownika, SIGTERM);
        if(kill_status_kierownik == -1) {
            if(errno != ESRCH) {
                showError("Blad wyslania SIGTERM do obslugi");
            }
        }
    }

    // Zakonczenie działania obłsugi
    if(obsluga_id > 0) {
        int kill_status_obsluga = kill(obsluga_id, SIGTERM);
        if(kill_status_obsluga == -1) {
            if(errno != ESRCH) {
                showError("Blad wyslania SIGTERM do obslugi");
            }
        }
    }

    // Zamkniecie kas
    komunikat << "[DYSKONT] " << "WYLACZENIE KAS" << "\n";
    for(int i=0; i < 8;i++) {
        int wynik = kill(stan_dyskontu->pid_kasy[i], SIGTERM);

        if(wynik == -1) {
            if(errno != ESRCH) {
                showError("Blad wyslania SIGTERM do obslugi");
            }
        }
    }

    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    if (errno != ECHILD) {
        checkError(-1, "Blad czekania na klientow");
    }
    
    if(stan_dyskontu->pid_kierownika > 0) {
        while(kill(stan_dyskontu->pid_kierownika, 0) == 0) {
            usleep(1000);
        }
    }

    delete lista_klientow;

    checkError( shmdt(dane_klientow), "Blad odlaczenia pamieci klientow" );
    checkError( shmdt(stan_dyskontu), "Blad odlaczenia pamieci stanu" );

    checkError( semctl(dyskont_sem_id, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( shmctl(shm_id_klienci, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( msgctl(msqid_kolejka_samo, IPC_RMID, NULL), "Blad zamkniecia kolejki_samoobslugowej" );
    checkError( msgctl(msqid_kolejka_stac1, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_1" );
    checkError( msgctl(msqid_kolejka_stac2, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    checkError( msgctl(msqid_kolejka_obsluga, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );

    return 0;
}
