#include <iostream>
#include <cstdlib> 
#include <ctime> 
#include <unistd.h>

using namespace std;

int randomTime(int time) {
    return rand() % time; 
}


int main() {
    srand(time(0));

    int time = randomTime(10);

    cout << "Klient wchodzi do sklepu" << endl;

    // ILES CZASU JEST W SKLEPIE 
    sleep(time);

    cout << "Klient wychodzi po " << time << " sekund" << endl;

    exit(0);
}