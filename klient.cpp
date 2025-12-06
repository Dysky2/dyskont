#include <iostream>
#include <cstdlib> 
#include <ctime> 

#include "utils.h"

using namespace std;

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

    int semid_klienci = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_stac1 = atoi(argv[4]);
    int msqid_kolejka_stac2 = atoi(argv[5]);
    int semid_kolejki = atoi(argv[6]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    struct sembuf operacjaP = {0, 1, 0};
    checkError( semop(semid_klienci, &operacjaP, 1), "Blad podniesienia semafora" );

    int time = randomTime(6);

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

    klientWzor klient = {getpid(), getpid(), 3, startowyNr, {"Pomidor", "Ananas", "Wino"}};
    alarm(10);

    zmien_wartosc_kolejki(semid_kolejki, lista_kas, startowyNr, 1);

    int status = msgsnd(startoweId, &klient, sizeof(klientWzor) - sizeof(long int), 0);
    cout << "Klient " << getpid() << " staje do kolejki " << startowyNr << endl;

    if(status != -1) {
        cout << "Klient idzie do kasy o nr: " << startowyNr << endl;
    }

    cout << "WARTOSC SEMAFORA W KLIENT: " << semctl(semid_klienci, 0, GETVAL) << endl;

    exit(0);
}