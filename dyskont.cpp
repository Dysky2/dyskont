#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>

#include "utils.h"

using namespace std;

int main() {
    srand(time(0));

    komunikat << "Pid dyskontu: " << getpid() << "\n";

    const double simulation_speed = 1.0;
    const int simulation_time = 30;
    const int startowa_ilosc_kas = 3;
    int ilosc_otwratych_kas = 0;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    komunikat << "---------------------------------------" << "\n";
    komunikat << "---    Dyskont otwraty zapraszamy   ---" << "\n";
    komunikat << "---------------------------------------" << "\n\n";

    // Tworze grupe semaforowa składająca sie z  semaforów
	key_t key = ftok(".", 'A');
    int semid_klienci = checkError( semget(key, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    key_t key_kolejek = ftok(".", 'H');
    int semid_kolejek = checkError( semget(key_kolejek, 3, IPC_CREAT|0600), "Blad utworzenie grupy semaforow");

    semctl(semid_kolejek, 0, SETVAL, 1);
    semctl(semid_kolejek, 1, SETVAL, 1);
    semctl(semid_kolejek, 2, SETVAL, 1);

    // Pamiec dzielona dla kas
    key_t key_kasy = ftok(".", 'B');
    int shmId_kasy = checkError( shmget(key_kasy, sizeof(kasy), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    key_t key_kolelejki = ftok(".", 'C');
    int semid_kolejki = checkError( semget(key_kolelejki, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    // Ustawienie wartosci semafora odrazu na 1 
    semctl(semid_kolejki, 0, SETVAL, startowa_ilosc_kas);

    kasy * lista_kas = (kasy *) shmat(shmId_kasy, NULL, 0);

    lista_kas->sredni_czas_obslugi[0] = CZAS_OCZEKIWANIA_NA_KOLEJKE_SAMOOBSLUGOWA;
    lista_kas->sredni_czas_obslugi[1] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;
    lista_kas->sredni_czas_obslugi[2] = CZAS_OCZEKIWANIA_NA_KOLEJKE_STACJONARNA;

    for(int i=0; i<8; i++) {
        if(i < 3) {
            lista_kas->dlugosc_kolejki[i] = 0;
        }
        lista_kas->pid_kasy[i] = 0;
        lista_kas->status[i] = 0;
    }

    // kolejka do kas samoobsługowych
    key_t key_kolejka = ftok(".", 'D');
    int msqid_kolejka_samo = checkError( msgget(key_kolejka, IPC_CREAT|0601) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kasy stacjonarnej_1
    key_t key_kolejka_stac1 = ftok(".", 'E');
    int msqid_kolejka_stac1 = checkError( msgget(key_kolejka_stac1, IPC_CREAT|0602) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kas stacjonarnej_2
    key_t key_kolejka_stac2 = ftok(".", 'F');
    int msqid_kolejka_stac2 = checkError( msgget(key_kolejka_stac2, IPC_CREAT|0603) , "Blad tworzenia kolejki komunikatow");

    // kolejka do obsługi 
    key_t key_kolejka_obsluga = ftok(".",'G');
    int msqid_kolejka_obsluga = checkError( msgget(key_kolejka_obsluga, IPC_CREAT|0604) , "Blad tworzenia kolejki komunikatow");

    int kieroid = checkError(fork(), "Blad utowrzenia forka");
    if (kieroid == 0) {
        checkError(
                execlp("tmux", 
                        "tmux", 
                        "split-window",
                        "-v",
                        "./kierownik",  
                        to_string(shmId_kasy).c_str(),
                        NULL
                ),
                "Blad wywolania execa"
            );
    }

    komunikat << "[DYSKONT]" << "Obsluga wchodzi do sklepu" << "\n";
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

    // Tworze wszytkie kasy i zostawiam otwarte tylko 3 samoobslugowe
    for (int i=0;i<8;i++) {
        if(i < 6) {
            int pid = checkError( fork(), "Blad utowrzenia forka");
            if ( pid == 0 ) {
                checkError( 
                    execl(
                        "./kasa", "kasa", 
                        to_string(semid_klienci).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_samo).c_str(),
                        to_string(semid_kolejki).c_str(), 
                        to_string(msqid_kolejka_obsluga).c_str(),
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                if( i < startowa_ilosc_kas) {
                    lista_kas->pid_kasy[i] = pid;
                    lista_kas->status[i]= 1;
                    ilosc_otwratych_kas++;
                } else {
                    lista_kas->pid_kasy[i] = pid;
                    lista_kas->status[i]= 0;
                }
            }
        } else {
            int pid = checkError( fork(), "Blad utowrzenia forka");
            if ( pid == 0 ) {
                checkError( 
                    execl(
                        "./kasjer", "kasjer", 
                        to_string(semid_klienci).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_stac1).c_str(), 
                        to_string(msqid_kolejka_stac2).c_str(), 
                        to_string(semid_kolejki).c_str(),
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                lista_kas->pid_kasy[i] = pid;
                lista_kas->status[i] = 0;
            }
        }
    }

    sleep(1);
    while( chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(3 / simulation_speed));

        if (chrono::steady_clock::now() >= end_simulation) {
            break;
        }

        int id = checkError( fork() , "Blad forka");
        if(id==0) {
            checkError( 
                execl("./klient", "klient", 
                    to_string(semid_klienci).c_str(), 
                    to_string(shmId_kasy).c_str(), 
                    to_string(msqid_kolejka_samo).c_str(), 
                    to_string(msqid_kolejka_stac1).c_str(), 
                    to_string(msqid_kolejka_stac2).c_str(), 
                    to_string(semid_kolejki).c_str(),
                    to_string(semid_kolejek).c_str(),
                    NULL
                ), 
                "Blad exec" 
            );
        } else {
            checkError( waitpid(-1, NULL, WNOHANG), "Bledne zebranie dziecka");
        }

        // otwarcie kasy stacjonarnej 1
        if(lista_kas->dlugosc_kolejki[0] > 3 && lista_kas->status[6] == 0) {
            lista_kas->status[6] = 1;
            struct sembuf operacjaV = {0, 1, SEM_UNDO};
            semop(semid_kolejki, &operacjaV, 1);
            kill(lista_kas->pid_kasy[6], SIGCONT);
        }

        // 5 10 15
        if(semctl(semid_klienci, 0, GETVAL) < 5 * (ilosc_otwratych_kas - 3)) {
            komunikat << "ZAMYKAM KASE " << "\n" << "\n";
            for(int i=6;i> 0;i--) {
                if(lista_kas->status[i] == 1) {
                    Klient klient = {2, lista_kas->pid_kasy[i]};
                    klient.ilosc_produktow = -1;
                    lista_kas->status[i]=0;
                    lista_kas->pid_kasy[i]=0;

                    struct sembuf operacjaP = {0, -1, SEM_UNDO};
                    semop(semid_kolejki, &operacjaP, 1);
                    msgsnd(msqid_kolejka_samo, &klient, sizeof(klient)-sizeof(long), 0);
                    ilosc_otwratych_kas--;
                    break;
                }
            }
        }

        // 15 20 25
        if (semctl(semid_klienci, 0, GETVAL) >= ilosc_otwratych_kas * 5 && ilosc_otwratych_kas <= 5) {

            int wolny_status = -1;
            for(int i =0;i <6;i++) {
                if (lista_kas->status[i] == 0) {
                    wolny_status = i;
                    break;
                }
            }
            if(wolny_status != -1) {
                ilosc_otwratych_kas++;
                lista_kas->status[wolny_status] = 1;
                struct sembuf operacjaV = {0, 1, SEM_UNDO};
                semop(semid_kolejki, &operacjaV, 1);
                kill(lista_kas->pid_kasy[wolny_status], SIGCONT);
            }
        }
        komunikat << "WARTOSC SEMAFORA: " << semctl(semid_klienci, 0, GETVAL) << "\n";

        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[0] << "\n";
        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[1] << "\n";
        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[2] << "\n";
    }

    // czekanie az klienci opuszcza sklep
    while (semctl(semid_klienci, 0, GETVAL) > 0) {
        komunikat << "Ilosc ludzi w kolejce IN SEM " << semctl(semid_klienci, 0, GETVAL) << "\n";
        sleep(2);
    }

    // Zakonczenie działania obłsugi
    Obsluga obsluga = {1, -1, -1};
    checkError( msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(obsluga) - sizeof(long int), 0), "Blad zamkniecia oblsugi" );

    // Wyslanie wiadomosci o zamkniecie kas
    komunikat << "[DYSKONT]" << "WYLACZENIE KAS" << "\n";
    for(int i=0; i < 8;i++) {
        int rcvId = msqid_kolejka_samo;
        if(i == 6) {
            rcvId = msqid_kolejka_stac1;
        }
        if(i == 7) {
            rcvId = msqid_kolejka_stac2;
        }
        if(lista_kas->status[i] == 1) {
            Klient klient = {1, -1};
            klient.ilosc_produktow = -1;
            checkError( 
                msgsnd(rcvId, &klient, sizeof(klient)- sizeof(long), 0) , 
                "Blad wyslania wiadomosci od klienta"
            );
        } else if(lista_kas->status[i] == 0) {
            kill(lista_kas->pid_kasy[i], SIGTERM);
        }
    }

    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    if (errno != ECHILD) {
        checkError(-1, "Blad czekania na klientwo");
    }

    checkError( semctl(semid_klienci, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( semctl(semid_kolejki, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( semctl(semid_kolejek, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( msgctl(msqid_kolejka_samo, IPC_RMID, NULL), "Blad zamkniecia kolejki_samoobslugowej" );
    checkError( msgctl(msqid_kolejka_stac1, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_1" );
    checkError( msgctl(msqid_kolejka_stac2, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    checkError( msgctl(msqid_kolejka_obsluga, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    return 0;
}
