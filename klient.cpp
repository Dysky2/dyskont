#include <iostream>
#include <cstdlib> 
#include <ctime> 

#include "utils.h"

using namespace std;

volatile sig_atomic_t stop_loop = 0; 

void sigalrm_handler(int sig) { }

void generateProducts() {
    int how_many_products = randomTimeWithRange(3, 11);

    vector<string> lista_produktow;

    for(int i=0; i<how_many_products;i++) {
        int ran = randomTime(10);
        lista_produktow.push_back(products[ran][randomTime(products[ran].size())]);
    }

    cout << "Klient idzie do kasy z produktami: ";
    for( auto x: lista_produktow) {
        cout << x << ", ";
    }

    lista_produktow.clear();
}


int main(int argc, char * argv[]) {
    srand(time(0) + getpid());

    signal(SIGALRM, sigalrm_handler);

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_stac1 = atoi(argv[4]);
    int msqid_kolejka_stac2 = atoi(argv[5]);
    int semid_kolejki = atoi(argv[6]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    struct sembuf operacjaV = {0, 1, 0};
    checkError( semop(semid_klienci, &operacjaV, 1), "Blad podniesienia semafora" );

    int time = randomTime(10);

    cout << "Klient wchodzi do sklepu" << endl;

    // ILES CZASU JEST W SKLEPIE 
    sleep(time);
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
    // generateProducts();
    
    int startoweId = msqid_kolejka_samo;
    int startowyNr = 0;

    bool stac1 = lista_kas->status[6] == 1;
    bool stac2 = lista_kas->status[7] == 1;

    if((stac1 || stac2) && randomTimeWithRange(1, 100) > 95) {
        if(stac1 && stac2) {
            int num = randomTimeWithRange(1,2);
            if(num == 1) {
                startoweId = msqid_kolejka_stac1;
                startowyNr = 1;
            } else {
                startoweId = msqid_kolejka_stac2;
                startowyNr = 2;
            }
        } else if(stac1) {
            startoweId = msqid_kolejka_stac1;
            startowyNr = 1;
        } else if (stac2) {
            startoweId = msqid_kolejka_stac2;
            startowyNr = 2;
        }
    }

    klientWzor klient = {1 , getpid(), 3, startowyNr, {"Pomidor", "Ananas", "Wino"}};

    int aktualneId = startoweId;
    int aktualnyNr = startowyNr;
    
    // alarm(10);
    int status = msgsnd(startoweId, &klient, sizeof(klientWzor) - sizeof(long int), 0);
    cout << "Klient " << getpid() << " staje do kolejki " << startowyNr << endl;

    if(status != -1) {
        cout << "Klient idzie do kasy o nr: " << startowyNr << endl;
        zmien_wartosc_kolejki(semid_kolejki, lista_kas, startowyNr, 1);
    }

    while (1) {
        alarm(10);

        klientWzor odebrany;
        int rcvStatus = msgrcv(aktualneId, &odebrany, sizeof(klient) - sizeof(long int), getpid(), 0);

        if(rcvStatus != -1) {
            cout << "ZAKUPY DOKONANE przez " << getpid() << " ILOSC PRODUKTÓW " << odebrany.ilosc_produktow << endl;
            // stop_loop = 1;
            break; 
        }


        if(errno == EINTR) {
            int doceloweIdKolejki = -1;
            int docelowyNr = -1;

            if(aktualnyNr == 0) {
                if(lista_kas->status[6] == 1 && (lista_kas->liczba_ludzi[0] - 2 > lista_kas->liczba_ludzi[1]) ) {
                    doceloweIdKolejki = msqid_kolejka_stac1;
                    docelowyNr = 1;
                } else if(lista_kas->status[7] == 1 && (lista_kas->liczba_ludzi[0] - 2  > lista_kas->liczba_ludzi[2])) {
                    doceloweIdKolejki = msqid_kolejka_stac2;
                    docelowyNr = 2;
                }
            } else if(aktualneId == 1) {
                if(lista_kas->status[7] == 1 && (lista_kas->liczba_ludzi[1] - 2  > lista_kas->liczba_ludzi[2])) {
                    doceloweIdKolejki = msqid_kolejka_stac2;
                    docelowyNr = 2;
                }
            }

            if(docelowyNr != -1) {
                // int mogeOpuscic = msgrcv(aktualneId, &klient, sizeof(klient) - sizeof(long int), getpid(), IPC_NOWAIT);

                cout << "Sprawdzenie klienta " << klient.mtype << " " << klient.klient_id << endl;
                // if( mogeOpuscic != -1) {

                    
                    if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
                        cout << "KLIENT probuje odjac z kolejki gdzie jest 0" << endl << endl;  
                    } else {
                        zmien_wartosc_kolejki(semid_kolejki, lista_kas, aktualnyNr, -1);
                    }
                    zmien_wartosc_kolejki(semid_kolejki, lista_kas, docelowyNr, 1);
                    
                    klient.nrKasy=docelowyNr;
                    status = msgsnd(doceloweIdKolejki, &klient, sizeof(klient) - sizeof(long int), 0);
                    cout << "Klient " << klient.klient_id << " zmienia kolejke na " << docelowyNr << endl;

                    aktualneId = doceloweIdKolejki;
                    aktualnyNr = docelowyNr;
                // }
            }

        }
    }

    cout << "WARTOSC SEMAFORA W KASIE - 1: " << semctl(semid_klienci, 0, GETVAL) << endl;       
    struct sembuf operacjaP = {0, -1, 0};
    checkError( semop(semid_klienci, &operacjaP, 1 ), "Blad obnizenia semafora" );
    cout << "WARTOSC SEMAFORA W KASIE - 2: " << semctl(semid_klienci, 0, GETVAL) << endl;
    cout << "KONIEC KLIENTA" << endl << endl;
    
    exit(0);
}