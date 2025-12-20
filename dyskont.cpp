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
    komunikat << "[DYSKONT] ZARZADAM ZAMKNIECIE SKLEPU\n";

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmId_kasy, NULL, 0);
    DaneListyKlientow * dane_klientow = (DaneListyKlientow *) shmat(shm_id_klienci, NULL ,0);

    ewakuacja = 1;

    for(int i =0;i<8;i++) {
        stan_dyskontu->status_kasy[i] = 0;
        kill(stan_dyskontu->pid_kasy[i], SIGUSR2);
    }

    while(semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) > 0) {
        sleep(1);
        for(int i=0;i < dane_klientow->ilosc;i++) {
            usleep(1000000);
            int pid_do_ubica = dane_klientow->lista_klientow[i];
            if(pid_do_ubica > 0) kill(pid_do_ubica, SIGTERM);
        }

        while(waitpid(-1, NULL, WNOHANG) > 0); 
    }

    if(obsluga_id > 0) {
        int wynik = kill(obsluga_id, SIGTERM);
        if(wynik == -1) {
            if(errno != ESRCH) {
                perror("Blad wyslania SIGTERM do obslugi");
            }
        }
    }

    for(int i =0;i<8;i++) {
        int wynik = kill(stan_dyskontu->pid_kasy[i], SIGTERM);

        if(wynik == -1) {
            if(errno != ESRCH) {
                perror("Blad wyslania SIGTERM do obslugi");
            }
        }
    }
}

int main() {
    srand(time(0) + getpid());

    if(signal(SIGCHLD, usun_zombie) == SIG_ERR) {
        perror("[DYSKONT] Sygnal SIGCHLD nie zostal wykonany poprawnie");
        exit(EXIT_FAILURE);
    }

    if(signal(SIGTERM, wylacz_sklep) == SIG_ERR) {
        perror("[DYSKONT] Sygnal SIGTERM nie zostal wykonany poprawnie");
        exit(EXIT_FAILURE);
    }

    dyskont_sem_id = utworz_grupe_semaforowa();
    ustaw_poczatkowe_wartosci_semaforow(dyskont_sem_id);
    
    // Tworze pamiec dzielona dla stanu calego dyskontu
    shm_id_klienci = checkError( shmget(utworz_klucz('B'), sizeof(DaneListyKlientow), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej dla klientow" );

    DaneListyKlientow * dane_klientow = (DaneListyKlientow *) shmat(shm_id_klienci, NULL ,0);

    //Tworze pamiec dzielona dla kas
    shmId_kasy = checkError( shmget(utworz_klucz('C'), sizeof(StanDyskontu), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    // Ustawienie wartosci semafora odrazu na ilosc_kas_startowych 
    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmId_kasy, NULL, 0);
    stan_dyskontu->sredni_czas_obslugi[0] = CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA;
    stan_dyskontu->sredni_czas_obslugi[1] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;
    stan_dyskontu->sredni_czas_obslugi[2] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;

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

    komunikat << "[Dyskont] Pid dyskontu: " << getpid() << "\n";
    komunikat << "---------------------------------------" << "\n";
    komunikat << "---    Dyskont otwraty zapraszamy   ---" << "\n";
    komunikat << "---------------------------------------" << "\n";

    int kierownik_pid = checkError(fork(), "Blad utowrzenia forka");
    if (kierownik_pid == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("Blad getcwd");
            exit(1);
        }
    
        string komenda_bash = "cd " + string(cwd) + "; ./kierownik " + 
                            to_string(shmId_kasy) + " " + 
                            to_string(dyskont_sem_id) + " " + 
                            to_string(msqid_kolejka_stac1) + " " + 
                            to_string(msqid_kolejka_stac2) +  "; exec bash";
        
        execlp("cmd.exe", 
            "cmd.exe",
            "/c",
            "start", 
            "Okienko Kierownika",
            "wsl.exe",  
            "-d", "Ubuntu-22.04",
            "--",
            "bash", 
            "-c", 
            komenda_bash.c_str(),
            NULL
        );

        perror("Blad uruchomienia nowego okna");
        exit(1);
    }

    komunikat << "[DYSKONT] Obsluga wchodzi do sklepu" << "\n";
    int opid = checkError(fork(), "Blad utowrzenia forka");
    if (opid == 0) {
        checkError(
            execl(
                "./obsluga", 
                "obsluga",
                to_string(msqid_kolejka_obsluga).c_str(),
                NULL
            ),
            "Blad wywolania execa"
        );
    }
    obsluga_id = opid;

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
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                if( i < startowa_ilosc_kas) {
                    stan_dyskontu->pid_kasy[i] = pid;
                    stan_dyskontu->status_kasy[i]= 1;
                    ilosc_otwratych_kas++;
                } else {
                    stan_dyskontu->pid_kasy[i] = pid;
                    stan_dyskontu->status_kasy[i]= 0;
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
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                stan_dyskontu->pid_kasy[i] = pid;
                stan_dyskontu->status_kasy[i] = 0;
            }
        }
    }
    sleep(1);

    ListaKlientow * lista_klientow = new ListaKlientow(dane_klientow, 1);
    while( time(NULL) < dyskont_koniec && !ewakuacja) {
        while(dane_klientow->ilosc > MAX_ILOSC_KLIENTOW - 2 && dane_klientow->ilosc < MAX_ILOSC_KLIENTOW) {
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
                    NULL
                ), 
                "Blad exec" 
            );
        } else {
            operacja_p(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);
            lista_klientow->dodaj_klienta_do_listy(id);
            operacja_v(dyskont_sem_id, SEMAFOR_LISTA_KLIENTOW);

        }

        // otwarcie kasy stacjonarnej 1
        if(stan_dyskontu->dlugosc_kolejki[0] > 3 && stan_dyskontu->status_kasy[6] == 0) {
            stan_dyskontu->status_kasy[6] = 1;
            struct sembuf operacjaV = {SEMAFOR_ILOSC_KAS, 1, SEM_UNDO};
            semop(dyskont_sem_id, &operacjaV, 1);
            kill(stan_dyskontu->pid_kasy[6], SIGUSR2);
        }

        // 5 10 15
        if(semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) < 5 * (ilosc_otwratych_kas - 3)) {
            komunikat << "[DYSKONT] ZAMYKAM KASE " << "\n" << "\n";
            for(int i=6;i> 0;i--) {
                if(stan_dyskontu->status_kasy[i] == 1) {
                    Klient klient = {1, stan_dyskontu->pid_kasy[i], 0,0,-1,0};
                    stan_dyskontu->status_kasy[i]=0;
                    stan_dyskontu->pid_kasy[i]=0;

                    struct sembuf operacjaP = {SEMAFOR_ILOSC_KAS, -1, SEM_UNDO};
                    if(semop(dyskont_sem_id, &operacjaP, 1) == 0) {
                        msgsnd(msqid_kolejka_samo, &klient, sizeof(klient)-sizeof(long), 0);
                        ilosc_otwratych_kas--;
                    }
                    break;
                }
            }
        }

        // 15 20 25
        if (semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) >= ilosc_otwratych_kas * 5 && ilosc_otwratych_kas <= 5) {

            int wolny_status = -1;
            for(int i =0;i <6;i++) {
                if (stan_dyskontu->status_kasy[i] == 0) {
                    wolny_status = i;
                    break;
                }
            }
            if(wolny_status != -1) {
                ilosc_otwratych_kas++;
                stan_dyskontu->status_kasy[wolny_status] = 1;
                struct sembuf operacjaV = {SEMAFOR_ILOSC_KAS, 1, SEM_UNDO};
                semop(dyskont_sem_id, &operacjaV, 1);
                kill(stan_dyskontu->pid_kasy[wolny_status], SIGUSR1);
            }
        }
        komunikat << "[DYSKONT] WARTOSC SEMAFORA: " << semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";

        komunikat << "[DYSKONT] Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[0] << "\n";
        komunikat << "[DYSKONT] Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[1] << "\n";
        komunikat << "[DYSKONT] Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[2] << "\n";
    }

    // czekanie az klienci opuszcza sklep
    while (semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) > 0) {
        if(ewakuacja) break;
        komunikat << "[DYSKONT] Ilosc ludzi w kolejce IN SEM " << semctl(dyskont_sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";
        sleep(2);
    }

    if(!ewakuacja) {

        // Zakonczenie działania obłsugi
        Obsluga obsluga = {1, -1, -1, 0, 0};
        checkError( msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(obsluga) - sizeof(long int), 0), "Blad zamkniecia oblsugi" );
        
        // Wyslanie wiadomosci o zamkniecie kas
        komunikat << "[DYSKONT]" << "WYLACZENIE KAS" << "\n";
        for(int i=0; i < 8;i++) {
            kill(stan_dyskontu->pid_kasy[i], SIGTERM);
        }
    }

    kill(stan_dyskontu->pid_kierownika, SIGTERM);

    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    if (errno != ECHILD) {
        checkError(-1, "Blad czekania na klientow");
    }

    checkError( semctl(dyskont_sem_id, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( shmctl(shm_id_klienci, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( msgctl(msqid_kolejka_samo, IPC_RMID, NULL), "Blad zamkniecia kolejki_samoobslugowej" );
    checkError( msgctl(msqid_kolejka_stac1, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_1" );
    checkError( msgctl(msqid_kolejka_stac2, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    checkError( msgctl(msqid_kolejka_obsluga, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    return 0;
}
