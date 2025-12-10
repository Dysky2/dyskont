#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "utils.h"

using namespace std;

int main() {
    srand(time(0));

    cout << "Pid dyskontu: " << getpid() << endl;

    const double simulation_speed = 1.0;
    const int simulation_time = 60;
    const int startowa_ilosc_kas = 3;
    int ilosc_otwratych_kas = 0;

    auto start = chrono::steady_clock::now();
    auto end_simulation = start + chrono::seconds( (int) (simulation_time / simulation_speed) );

    cout << "---------------------------------------" << endl;
    cout << "---    Dyskont otwraty zapraszamy   ---" << endl;
    cout << "---------------------------------------" << endl << endl;

    // Ilczba klientow w sklepie
	key_t key = ftok(".", 'A');
    int semid_klienci = checkError( semget(key, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    // Pamiec dzielona dla kas
    key_t key_kasy = ftok(".", 'B');
    int shmId_kasy = checkError( shmget(key_kasy, sizeof(kasy), IPC_CREAT|0600), "Blad tworzenia pamieci wspoldzielonej");

    key_t key_kolelejki = ftok(".", 'F');
    int semid_kolejki = checkError( semget(key_kolelejki, 1, IPC_CREAT|0600), "Blad tworzenie semafora");

    // Ustawienie wartosci semafora odrazu na 1 
    union semun argument;
    argument.val = 1;
    semctl(semid_kolejki, 0, SETVAL, argument);

    kasy * lista_kas = (kasy *) shmat(shmId_kasy, NULL, 0);

    for(int i=0; i<8; i++) {
        if(i ==0 || i == 1 | i == 2) {
            lista_kas->liczba_ludzi[i] = 0;
        }
        lista_kas->pid_kasy[i] = 0;
        lista_kas->status[i] = 0;
    }

    // kolejka do kas samoobsługowych
    key_t key_kolejka = ftok(".", 'C');
    int msqid_kolejka_samo = checkError( msgget(key_kolejka, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kasy stacjonarnej_1
    key_t key_kolejka_stac1 = ftok(".", 'D');
    int msqid_kolejka_stac1 = checkError( msgget(key_kolejka_stac1, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    // kolejka do kas stacjonarnej_2
    key_t key_kolejka_stac2 = ftok(".", 'E');
    int msqid_kolejka_stac2 = checkError( msgget(key_kolejka_stac2, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");


    cout << "Otwieram kasy samoobsługowe" << endl;
    for(int i=0;i<3;i++) {
        int pid = checkError( fork(), "Blad utowrzenia forka");
        if ( pid == 0 ) {
            checkError( 
                execl(
                    "./kasa", "kasa", 
                    to_string(semid_klienci).c_str(), 
                    to_string(shmId_kasy).c_str(), 
                    to_string(msqid_kolejka_samo).c_str(),
                    to_string(semid_kolejki).c_str(), 
                    NULL
                ),
                "Blad wywolania execa"
            );
        } else {
            if( i < startowa_ilosc_kas) {
                lista_kas->pid_kasy[i] = pid;
                lista_kas->status[i]= 1;
                ilosc_otwratych_kas++;
            }
        }
    }

    while( chrono::steady_clock::now() < end_simulation ) {
        sleep(randomTime(5 / simulation_speed));

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
                    NULL
                ), 
                "Blad exec" 
            );
        } else {
            checkError( waitpid(-1, NULL, WNOHANG), "Bledne zebranie dziecka");
        }

        // otwarcie kasy stacjonarnej 1
        if(lista_kas->liczba_ludzi[0] > 3 && lista_kas->status[6] == 0) {
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
                lista_kas->status[6] = 1;
                lista_kas->pid_kasy[6] = pid;
            }
        }

        // 5 10 15
        if(semctl(semid_klienci, 0, GETVAL) < 5 * (ilosc_otwratych_kas - 3)) {
            cout << "ZAMYKAM KASE " << endl << endl;;
            for(int i=6;i> 0;i--) {
                if(lista_kas->status[i] == 1) {
                    klientWzor klient = {lista_kas->pid_kasy[i], lista_kas->pid_kasy[i], -1};
                    lista_kas->status[i]=0;
                    lista_kas->pid_kasy[i]=0;
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
                int pid = checkError( fork(), "Blad utowrzenia forka");
                if ( pid == 0 ) {
                    checkError( 
                        execl(
                            "./kasa", "kasa", 
                            to_string(semid_klienci).c_str(), 
                            to_string(shmId_kasy).c_str(), 
                            to_string(msqid_kolejka_samo).c_str(),
                            to_string(semid_kolejki).c_str(),    
                            NULL
                        ),
                        "Blad wywolania execa"
                    );
                } else {
                    lista_kas->pid_kasy[wolny_status] = pid;
                    lista_kas->status[wolny_status] = 1;
                    ilosc_otwratych_kas++;
                }
            }
        }
        cout << "WARTOSC SEMAFORA: " << semctl(semid_klienci, 0, GETVAL) << endl;

        cout << "Ilosc ludzi w kolejce " << lista_kas->liczba_ludzi[0] << endl;
        cout << "Ilosc ludzi w kolejce " << lista_kas->liczba_ludzi[1] << endl;
        cout << "Ilosc ludzi w kolejce " << lista_kas->liczba_ludzi[2] << endl;
    }

    // czekanie az klienci opuszcza sklep
    while (semctl(semid_klienci, 0, GETVAL) > 0) {
        cout << "WARTOSC SEMAFORA: " << semctl(semid_klienci, 0, GETVAL) << endl;
        sleep(2);
    }

    // Wyslanie wiadomosci o zamkniecie kas
    cout << "WYLACZENIE KAS" << endl;
    for(int i=0; i < 8;i++) {
        int rcvId = msqid_kolejka_samo;
        if(i == 6) {
            rcvId = msqid_kolejka_stac1;
        }
        if(i == 7) {
            rcvId = msqid_kolejka_stac2;
        }
        if(lista_kas->status[i] == 1) {
            klientWzor klient = {1, -1, -1};
            checkError( 
                msgsnd(rcvId, &klient, sizeof(klient)- sizeof(long), 0) , 
                "Blad wyslania wiadomosci od klienta"
            );
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
    checkError( msgctl(msqid_kolejka_samo, IPC_RMID, NULL), "Blad zamkniecia kolejki_samoobslugowej" );
    checkError( msgctl(msqid_kolejka_stac1, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_1" );
    checkError( msgctl(msqid_kolejka_stac2, IPC_RMID, NULL), "Blad zamkniecia kolejki stacjonarnej_2" );

    return 0;
}
