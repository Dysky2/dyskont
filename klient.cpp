#include <iostream>
#include <cstdlib> 
#include <ctime> 

#include "utils.h"

using namespace std;

volatile sig_atomic_t stop_loop = 0; 

void sigalrm_handler(int sig) { }

// Generuje miedzy 3-10 produktów z 10 kategori i przypisuje do wskaznika na strukture Klient
// Wpisuje ilosc_produków i liste
int generateProducts(Klient *klient){
    int how_many_products = randomTimeWithRange(3, 10);

    klient->ilosc_produktow = how_many_products;

    const char *lista_produktow[how_many_products];
    for(int i=0; i<how_many_products;i++) {
        int ran = randomTime(10);
        lista_produktow[i] = products[ran][randomTime(products[ran].size())].data();
    }

    int offset = 0;
    for(int i=0;i < how_many_products; i++) {
        int len = strlen(lista_produktow[i]) + 1;

        if(offset + len > MAX_DATA_SIZE) {
            perror("Za duzo danych");
            break;
        }

        strcpy(klient->lista_produktow + offset, lista_produktow[i]);
        offset += len;
    }
    return offset;
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

    komunikat << "Klient wchodzi do sklepu" << "\n";
    
    // ILES CZASU JEST W SKLEPIE 
    sleep(randomTime(10));
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

    int aktualneId = startoweId;
    int aktualnyNr = startowyNr;

    Klient klient = {1 , getpid(), aktualnyNr};
    klient.wiek = randomTimeWithRange(8, 50);
    int dlugosc_tekstu = generateProducts(&klient);
    
    // alarm(10);
    zmien_wartosc_kolejki(semid_kolejki, lista_kas, aktualnyNr, 1);
    int status = msgsnd(startoweId, &klient, sizeof(int) * 4 + dlugosc_tekstu, 0);
    komunikat << "Klient " << getpid() << " staje do kolejki " << aktualnyNr << "\n"; 

    if(status != -1) {
        komunikat << "Klient idzie do kasy o nr: " << aktualnyNr << "\n";
    } else {
        zmien_wartosc_kolejki(semid_kolejki, lista_kas, aktualnyNr, -1);
    }

    komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->liczba_ludzi[0] << "\n";
    komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->liczba_ludzi[1] << "\n";
    komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->liczba_ludzi[2] << "\n";

    while (1) {
        alarm(10);

        Klient odebrany;
        int rcvStatus = msgrcv(aktualneId, &odebrany, sizeof(klient) - sizeof(long int), getpid(), 0);

        if(rcvStatus != -1) {
             komunikat << "ZAKUPY DOKONANE przez " << getpid() << " ILOSC PRODUKTÓW " << odebrany.ilosc_produktow << "\n";
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

                komunikat << "Sprawdzenie klienta " << klient.mtype << " " << klient.klient_id << "\n";
                // if( mogeOpuscic != -1) {
                    
                    if(lista_kas->liczba_ludzi[klient.nrKasy] <= 0) {
                        komunikat << "KLIENT probuje odjac z kolejki gdzie jest 0" << "\n" << "\n";
                    } else {
                        zmien_wartosc_kolejki(semid_kolejki, lista_kas, aktualnyNr, -1);
                    }
                    zmien_wartosc_kolejki(semid_kolejki, lista_kas, docelowyNr, 1);
                    
                    klient.nrKasy=docelowyNr;
                    status = msgsnd(doceloweIdKolejki, &klient,  sizeof(int) * 4 + dlugosc_tekstu, 0);
                    cout << "Klient " << klient.klient_id << " zmienia kolejke na " << docelowyNr << endl;

                    aktualneId = doceloweIdKolejki;
                    aktualnyNr = docelowyNr;
                // }
            }

        }
    }

    komunikat << "WARTOSC SEMAFORA- 1: " << semctl(semid_klienci, 0, GETVAL) << "\n";       
    struct sembuf operacjaP = {0, -1, 0};
    checkError( semop(semid_klienci, &operacjaP, 1 ), "Blad obnizenia semafora" );
    komunikat << "WARTOSC SEMAFORA- 2: " << semctl(semid_klienci, 0, GETVAL) << "\n";
    komunikat << "KONIEC KLIENTA" << "\n" << "\n";
    
    exit(0);
}