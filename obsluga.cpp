#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    komunikat << "Witam tutaj obłsuga " << "\n";

    int msqid_kolejka_obsluga = atoi(argv[1]);

    while(1) {
        Obsluga obsluga;
        msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 1, 0);

        if(obsluga.powod == -1) {
            break;
        }

        sleep(5);
        if(obsluga.powod == 1) {
            if(obsluga.wiek_klienta >= 18) {
                obsluga.pelnoletni = 0;
                komunikat << "Klient jest pelnoletni, moze kupic alkohol, przy kasie: " << obsluga.kasa_id << "\n";
            } else {
                obsluga.pelnoletni = -1;
                komunikat << "Klient nie jest pelnoletni, alkohol zostanie odlozony, przy kasie: " << obsluga.kasa_id  << "\n";
            }
        }
        obsluga.mtype = obsluga.kasa_id;
        msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0);
    }

    komunikat << "Koniec obsługi " << "\n";
    exit(0);
}