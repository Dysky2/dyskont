#include <iostream>
#include <cstdlib> 
#include <ctime> 
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "utils.h"

using namespace std;

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
    int msqid_kolejka = atoi(argv[3]);

    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    struct sembuf operacjaP = {0, 1, 0};
    checkError( semop(semid_klienci, &operacjaP, 1), "Blad podniesienia semafora" );

    int time = randomTime(6);

    cout << "Klient wchodzi do sklepu" << endl;

    // ILES CZASU JEST W SKLEPIE 
    sleep(time);
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
    // generateProducts();

    klientWzor klient = {1, getpid(), 3, {"Pomidor", "Ananas", "Wino"}}; 

    checkError( msgsnd(msqid_kolejka, &klient, sizeof(klient) - sizeof(long), 0), "Blad wyslania wiadomosci od klienta"); 

    cout << "Klient Idzie do kasy " << time << " sekund, jego pid: " << getpid() << endl;
    lista_kas->liczba_ludzi[0]++;

    exit(0);
}