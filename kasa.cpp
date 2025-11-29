#include <iostream>
#include <unistd.h>
#include <sys/msg.h>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    cout << "Tutaj kasa witam" << endl;

    klientWzor klient;
    checkError( msgrcv(atoi(argv[1]), &klient, sizeof(klient) - sizeof(long), 0, 0), "Blad odebrania wiadomosci" );

    cout << "ODEBRANO KOMUNIKAT " << klient.klient_id << " Z TEJ STRONY: " << getpid() << endl;
    sleep(3);

    cout << "Koniec kasy " << endl;
    exit(0);
}
