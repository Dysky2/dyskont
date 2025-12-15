#include <iostream>

#include "utils.h"

using namespace std;

int main(int argc, char * argv[]) {
    komunikat << "[OBSLUGA] " << "Witam " << "\n";

    int msqid_kolejka_obsluga = atoi(argv[1]);

    while(1) {
        Obsluga obsluga;
        msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 1, 0);

        if(obsluga.powod == -1) {
            break;
        }

        if(obsluga.powod == 1) {
            // Czas az obsluga podejdzie do kasy
            sleep(2);
            if(obsluga.wiek_klienta >= 18) {
                obsluga.pelnoletni = 0;
                komunikat << "[OBSLUGA] " << "Klient jest pelnoletni, moze kupic alkohol, przy kasie: " << obsluga.kasa_id << "\n";
            } else {
                obsluga.pelnoletni = -1;
                komunikat << "[OBSLUGA] " << "Klient nie jest pelnoletni, alkohol zostanie odlozony, przy kasie: " << obsluga.kasa_id  << "\n";
            }
        } else if(obsluga.powod == 2) {
            // Czas az obsługa poprawi kase
            sleep(2);
            komunikat << "[OBSLUGA] " << "Waga przy kasie " << obsluga.kasa_id << " zostala naprawiona\n";
        }


        obsluga.mtype = obsluga.kasa_id;
        msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0);
    }

    komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
    exit(0);
}