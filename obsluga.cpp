#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_obsluguje = 1; 

void opusc_sklep(int) {
    czy_obsluguje = 0;
}

int main(int, char * argv[]) {
    komunikat << "[OBSLUGA] " << "Witam " << "\n";

    signal(SIGTERM, opusc_sklep);

    int msqid_kolejka_obsluga = atoi(argv[1]);

    while(czy_obsluguje) {
        Obsluga obsluga;
        int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 1, 0);

        if(obsluga.powod == -1) {
            komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
            exit(0);
        }

        if(rcvStatus == -1) {
            if (errno == EINTR) {
                break;
            } else if(errno == EIDRM || errno == EINVAL) {
                break;
            } else {
                perror("Blad podczas odbierania komuniaktu z kolejki");
                exit(EXIT_FAILURE);
            }
        }

        if(obsluga.powod == 1) {
            // Czas az obsluga podejdzie do kasy
            sleep(6);
            if(!czy_obsluguje) {
                komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
                exit(0);
            }

            if(obsluga.wiek_klienta >= 18) {
                obsluga.pelnoletni = 0;
                komunikat << "[OBSLUGA] " << "Klient jest pelnoletni, moze kupic alkohol, przy kasie: " << obsluga.kasa_id << "\n";
            } else {
                obsluga.pelnoletni = -1;
                komunikat << "[OBSLUGA] " << "Klient nie jest pelnoletni, alkohol zostanie odlozony, przy kasie: " << obsluga.kasa_id  << "\n";
            }
        } else if(obsluga.powod == 2) {
            // Czas az obsługa poprawi kase
            sleep(6);  
            
            if(!czy_obsluguje) {
                komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
                exit(0);
            }
            komunikat << "[OBSLUGA] " << "Waga przy kasie " << obsluga.kasa_id << " zostala naprawiona\n";
        }


        obsluga.mtype = obsluga.kasa_id;
        if(kill(obsluga.kasa_id, 0) == 0) {
            msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0);
        }
    }

    komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
    exit(0);
}