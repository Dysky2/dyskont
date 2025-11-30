#include <iostream>
#include <cstdlib> 
#include <ctime> 
#include <unistd.h>
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

    struct sembuf operacjaP = {0, -1, SEM_UNDO};

    int time = randomTime(10);

    cout << "Klient wchodzi do sklepu" << endl;

    // ILES CZASU JEST W SKLEPIE 
    sleep(time);
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
    // generateProducts();

    klientWzor klient = {2, getpid(), 3, {"Pomidor", "Ananas", "Wino"}}; 

    msgsnd(atoi(argv[2]), &klient, sizeof(klient) - sizeof(long), 0);

    cout << "Klient Idzie do kasy " << time << " sekund, jego pid: " << getpid() << endl;

    // checkError(semop(stoi(argv[1]), &operacjaP, 1), "Blad obnizenia semafora" );

    exit(0);
}