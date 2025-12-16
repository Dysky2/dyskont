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

    int ilosc_otwratych_kas = 0;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    komunikat << "---------------------------------------" << "\n";
    komunikat << "---    Dyskont otwraty zapraszamy   ---" << "\n";
    komunikat << "---------------------------------------" << "\n\n";

	key_t key_sem = ftok(".", 'A');
    key_t key_kasy = ftok(".", 'B');
    key_t key_kolejka_samo = ftok(".", 'C');
    key_t key_kolejka_stac1 = ftok(".", 'D');
    key_t key_kolejka_stac2 = ftok(".", 'E');
    key_t key_kolejka_obsluga = ftok(".",'F');
    
    // Tworze grupe semaforowa składająca sie z  semaforów
    // 0 -> semafor do koleki samooboslugowej
    // 1 -> semafor do koleki stacjonarnej 1
    // 2 -> semafor do koleki stacjonarnej 2
    // 3 -> semafor liczaczy ilosc klientow w sklepie 
    // 4 -> semafor odpowiadajacy ile kas aktualnie jest otwartych
    int sem_id = checkError( semget(key_sem, 5, IPC_CREAT|0600), "Blad utworzenia semaforow" );

    // ustawien wartosc poczatkowych dla semaforow
    semctl(sem_id, SEMAFOR_SAMOOBSLUGA, SETVAL, 1);
    semctl(sem_id, SEMAFOR_STAC1, SETVAL, 1);
    semctl(sem_id, SEMAFOR_STAC2, SETVAL, 1);
    semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, SETVAL, 0);
    semctl(sem_id, SEMAFOR_ILOSC_KAS, SETVAL, startowa_ilosc_kas);

    //Tworze pamiec dzielona dla kas
    int shmId_kasy = checkError( shmget(key_kasy, sizeof(kasy), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    // Ustawienie wartosci semafora odrazu na ilosc_kas_startowych 
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
    int msqid_kolejka_samo = checkError( msgget(key_kolejka_samo, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kasy stacjonarnej_1
    int msqid_kolejka_stac1 = checkError( msgget(key_kolejka_stac1, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kas stacjonarnej_2
    int msqid_kolejka_stac2 = checkError( msgget(key_kolejka_stac2, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do obsługi 
    int msqid_kolejka_obsluga = checkError( msgget(key_kolejka_obsluga, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    int kierownik_pid = checkError(fork(), "Blad utowrzenia forka");
    if (kierownik_pid == 0) {
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
                        to_string(sem_id).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_samo).c_str(),
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
                        to_string(sem_id).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka_stac1).c_str(), 
                        to_string(msqid_kolejka_stac2).c_str(), 
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
                    to_string(sem_id).c_str(), 
                    to_string(shmId_kasy).c_str(), 
                    to_string(msqid_kolejka_samo).c_str(), 
                    to_string(msqid_kolejka_stac1).c_str(), 
                    to_string(msqid_kolejka_stac2).c_str(), 
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
            struct sembuf operacjaV = {SEMAFOR_ILOSC_KAS, 1, SEM_UNDO};
            semop(sem_id, &operacjaV, 1);
            kill(lista_kas->pid_kasy[6], SIGCONT);
        }

        // 5 10 15
        if(semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) < 5 * (ilosc_otwratych_kas - 3)) {
            komunikat << "ZAMYKAM KASE " << "\n" << "\n";
            for(int i=6;i> 0;i--) {
                if(lista_kas->status[i] == 1) {
                    Klient klient = {2, lista_kas->pid_kasy[i]};
                    klient.ilosc_produktow = -1;
                    lista_kas->status[i]=0;
                    lista_kas->pid_kasy[i]=0;

                    struct sembuf operacjaP = {SEMAFOR_ILOSC_KAS, -1, SEM_UNDO};
                    semop(sem_id, &operacjaP, 1);
                    msgsnd(msqid_kolejka_samo, &klient, sizeof(klient)-sizeof(long), 0);
                    ilosc_otwratych_kas--;
                    break;
                }
            }
        }

        // 15 20 25
        if (semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) >= ilosc_otwratych_kas * 5 && ilosc_otwratych_kas <= 5) {

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
                struct sembuf operacjaV = {SEMAFOR_ILOSC_KAS, 1, SEM_UNDO};
                semop(sem_id, &operacjaV, 1);
                kill(lista_kas->pid_kasy[wolny_status], SIGCONT);
            }
        }
        komunikat << "WARTOSC SEMAFORA: " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";

        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[0] << "\n";
        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[1] << "\n";
        komunikat << "Ilosc ludzi w kolejce " << lista_kas->dlugosc_kolejki[2] << "\n";
    }

    // czekanie az klienci opuszcza sklep
    while (semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) > 0) {
        komunikat << "Ilosc ludzi w kolejce IN SEM " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";
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

    checkError( semctl(sem_id, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");
    checkError( msgctl(msqid_kolejka_samo, IPC_RMID, NULL), "Blad zamkniecia kolejki_samoobslugowej" );
    checkError( msgctl(msqid_kolejka_stac1, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_1" );
    checkError( msgctl(msqid_kolejka_stac2, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    checkError( msgctl(msqid_kolejka_obsluga, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );
    return 0;
}
