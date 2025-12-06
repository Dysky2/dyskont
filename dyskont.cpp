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

    const double simulation_speed = 1.0;
    const int simulation_time = 30;
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

    kasy * lista_kas = (kasy *) shmat(shmId_kasy, NULL, 0);

    // kolejka do kas
    key_t key_kolejka = ftok(".", 'C');
    int msqid_kolejka = checkError( msgget(key_kolejka, IPC_CREAT|0600) , "Blad tworzenia kolejki komunikatow");

    cout << "Otwieram kasy samoobsługowe" << endl;
    for(int i=0;i<3;i++) {
        int pid = checkError( fork(), "Blad utowrzenia forka");
        if ( pid == 0 ) {
            checkError( 
                execl(
                    "./kasa", "kasa", 
                    to_string(semid_klienci).c_str(), 
                    to_string(shmId_kasy).c_str(), 
                    to_string(msqid_kolejka).c_str(),
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
        sleep(randomTime(2 / simulation_speed));

        int id = checkError( fork() , "Blad forka");
        if(id==0) {
            checkError( 
                execl("./klient", "klient", 
                    to_string(semid_klienci).c_str(), 
                    to_string(shmId_kasy).c_str(), 
                    to_string(msqid_kolejka).c_str(), 
                    NULL
                ), 
                "Blad exec" 
            );
        } else {
            checkError( waitpid(-1, NULL, WNOHANG), "Bledne zebranie dziecka");
        }

        // 5 10 15
        if(semctl(semid_klienci, 0, GETVAL) < 5 * (ilosc_otwratych_kas - 3)) {
            cout << "ZAMYKAM KASE " << endl << endl;;
            for(int i=6;i> 0;i--) {
                if(lista_kas->status[i] == 1) {
                    klientWzor klient = {2, lista_kas->pid_kasy[i], -1};
                    lista_kas->status[i]=0;
                    lista_kas->pid_kasy[i]=0;
                    msgsnd(msqid_kolejka, &klient, sizeof(klient)-sizeof(long), 0);
                    ilosc_otwratych_kas--;
                    break;
                }
            }
        }

        // 15 20 25
        if (semctl(semid_klienci, 0, GETVAL) >= ilosc_otwratych_kas * 5 && ilosc_otwratych_kas <= 5) {
            int pid = checkError( fork(), "Blad utowrzenia forka");
            if ( pid == 0 ) {
                checkError( 
                    execl(
                        "./kasa", "kasa", 
                        to_string(semid_klienci).c_str(), 
                        to_string(shmId_kasy).c_str(), 
                        to_string(msqid_kolejka).c_str(), 
                        NULL
                    ),
                    "Blad wywolania execa"
                );
            } else {
                for(int i=0;i<6;i++) {
                    if (lista_kas->status[i] == 0) {
                        lista_kas->pid_kasy[i] = pid;
                        lista_kas->status[i] = 1;
                        ilosc_otwratych_kas++;
                        break;
                    }
                }
            }
        }

        cout << "WARTOSC SEMAFORA: " << semctl(semid_klienci, 0, GETVAL) << endl;

        cout << "Ilosc ludzi w kolejce " << lista_kas->liczba_ludzi[0] << endl;
    }

    // czekanie az klienci opuszcza sklep
    while (semctl(semid_klienci, 0, GETVAL) > 0) {
        cout << "WARTOSC SEMAFORA: " << semctl(semid_klienci, 0, GETVAL) << endl;
        sleep(2);
    }

    // Wyslanie wiadomosci o zamkniecie kas
    for(int i=0; i < 8;i++) {
        if(lista_kas->status[i] == 1) {
            klientWzor klient;
            klient.mtype=1;
            klient.klient_id=-1;
            klient.ilosc_produktow=-1;

            checkError( 
                msgsnd(msqid_kolejka, &klient, sizeof(klient)- sizeof(long), 0), 
                "Blad wyslania wiadomosci od klienta"
            );
        }
    }

    // Czekam az wszyscy klienci zakoncza zakupy
    while (wait(NULL) > 0) {}

    if (errno != ECHILD) {
        checkError(-1, "Blad czekania na klientwo");
    }

    checkError(semctl(semid_klienci, 0, IPC_RMID), "Blad zamknieca semfora");
    checkError(msgctl(msqid_kolejka, IPC_RMID, NULL), "Blad zamknieca kolejki komunikatow");
    checkError( shmctl(shmId_kasy, IPC_RMID, NULL), "Blad zamkniecia pamieci dzielonj");

    return 0;
}
