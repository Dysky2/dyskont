#include <iostream>

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_obsluguje = 1; 

void opusc_sklep(int) {
    czy_obsluguje = 0;
}

int main(int, char * argv[]) {
    utworz_grupe_semaforowa();

    signal(SIGTERM, opusc_sklep);
    
    int msqid_kolejka_obsluga = atoi(argv[1]);
    string nazwa_katalogu = argv[2];
    
    ustaw_nazwe_katalogu(nazwa_katalogu);
    
    komunikat << "[OBSLUGA] " << "Witam " << "\n";


    while(czy_obsluguje) {
        Obsluga obsluga;
        int rcvStatus = msgrcv(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 1, 0);

        if(obsluga.powod == -1) {
            komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
            exit(0);
        }

        if( !czy_obsluguje ) {
            break;
        }

        if(rcvStatus == -1) {
            if (errno == EINTR && czy_obsluguje) {
                continue;
            } else {
                perror("Blad podczas odbierania komuniaktu z kolejki");
                exit(EXIT_FAILURE);
            }
        }

        if(obsluga.powod == 1) {
            // Czas az obsluga podejdzie do kasy
            sleep(6);

            if(!czy_obsluguje) {
                break;
            }
            
            if(obsluga.wiek_klienta >= 18) {
                obsluga.pelnoletni = 1;
                komunikat << "[OBSLUGA] " << "Klient jest pelnoletni, moze kupic alkohol, przy kasie: " << obsluga.kasa_id << "\n";
            } else {
                obsluga.pelnoletni = 0;
                komunikat << "[OBSLUGA] " << "Klient nie jest pelnoletni, alkohol zostanie odlozony, przy kasie: " << obsluga.kasa_id  << "\n";
            }
        } else if(obsluga.powod == 2) {
            // Czas az obsługa poprawi kase
            sleep(6);  

            if(!czy_obsluguje) {
                break;
            }

            komunikat << "[OBSLUGA] " << "Waga przy kasie " << obsluga.kasa_id << " zostala naprawiona\n";
        }

        if(!czy_obsluguje) {
            break;
        }

        obsluga.mtype = obsluga.kasa_id;
        if(kill(obsluga.kasa_id, 0) == 0) {
            checkError(msgsnd(msqid_kolejka_obsluga, &obsluga, sizeof(Obsluga) - sizeof(long int), 0), "Bledne wyslanie wiadomosci od obslugi do klienta");
        }
    }

    komunikat << "[OBSLUGA] " << "Koncze prace " << "\n";
    exit(0);
}