#include <iostream>
#include <cstdlib> 
#include <ctime> 

#include "utils.h"

using namespace std;

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

    int sem_id = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_stac1 = atoi(argv[4]);
    int msqid_kolejka_stac2 = atoi(argv[5]);
    
    kasy * lista_kas = (kasy *) shmat(shmid_kasy, NULL, 0);

    struct sembuf operacjaV_ilosc_klientow = {SEMAFOR_ILOSC_KLIENTOW, 1, SEM_UNDO};
    struct sembuf operacjaP_ilosc_klientow = {SEMAFOR_ILOSC_KLIENTOW, -1, SEM_UNDO};
    struct sembuf operacjaV_nr_kasy = {0, 1, SEM_UNDO};
    struct sembuf operacjaP_nr_kasy = {0, -1, SEM_UNDO};
    struct sembuf operacjaV_ilosc_kas = {SEMAFOR_ILOSC_KAS, 1, SEM_UNDO};
    struct sembuf operacjaP_ilosc_kas = {SEMAFOR_ILOSC_KAS, -1, SEM_UNDO};

    checkError( semop(sem_id, &operacjaV_ilosc_klientow, 1), "Blad podniesienia semafora" );

    komunikat << "[" << "KLIENT-" << getpid() << "] " << "Wchodzi do sklepu" << "\n";
    
    // ILES CZASU JEST W SKLEPIE 
    sleep(randomTime(5));
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
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

    Kolejka * kolejka = new Kolejka(lista_kas);

    operacjaP_nr_kasy.sem_num = aktualnyNr;
    operacjaV_nr_kasy.sem_num = aktualnyNr;

    checkError( semop(sem_id, &operacjaP_nr_kasy, 1), "Blad obnizenia semafora-1" );
    kolejka->dodaj_do_kolejki(getpid(), aktualnyNr);
    checkError( semop(sem_id, &operacjaV_nr_kasy, 1), "Blad podniesienia semafora-1" );
    komunikat << "[" << "Klient-" << getpid() << "] " << "Staje do kasy " << aktualnyNr << "\n";

    time_t czas_startu = time(NULL);
    while(1) {
        operacjaP_nr_kasy.sem_num = aktualnyNr;
        operacjaV_nr_kasy.sem_num = aktualnyNr;
        checkError( semop(sem_id, &operacjaP_nr_kasy, 1), "Blad obnizenia semafora-2" );
        int czy_jestem_pierwszy = kolejka->czy_jestem_pierwszy(getpid(), aktualnyNr);
        checkError( semop(sem_id, &operacjaV_nr_kasy, 1), "Blad podniesienia semafora-2" );

        if(czy_jestem_pierwszy) {
            komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->dlugosc_kolejki[0] << "\n";
            komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->dlugosc_kolejki[1] << "\n";
            komunikat << "Ilosc ludzi w kolejce OD KLIENTA " << lista_kas->dlugosc_kolejki[2] << "\n";

            checkError( semop(sem_id, &operacjaP_ilosc_kas, 1), "Blad obnizenia semafora-3" );

            Klient klient = {1 , getpid(), aktualnyNr};
            klient.wiek = randomTimeWithRange(8, 50);
            int dlugosc_tekstu = generateProducts(&klient);

            int status = msgsnd(aktualneId, &klient, sizeof(int) * 4 + dlugosc_tekstu, 0);
            if(status != -1) {
                checkError( semop(sem_id, &operacjaP_nr_kasy, 1), "Blad obnizenia semafora-2" );
                kolejka->usun_z_kolejki(getpid(), aktualnyNr);
                checkError( semop(sem_id, &operacjaV_nr_kasy, 1), "Blad podniesienia semafora-2" );

                komunikat << "[" << "KLIENT-" << getpid() << "] " << "Ide do kasy nr: " << aktualnyNr << "\n";
            }

            Klient odebrany;
            memset(&klient, 0, sizeof(Klient)); 
            int rcvStatus = msgrcv(aktualneId, &odebrany, sizeof(Klient) - sizeof(long int), getpid(), 0);

            if(rcvStatus != -1) {
                komunikat << "[" << "KLIENT-" << getpid() << "] " << "ZAKUPY DOKONANE ILOSC PRODUKTÓW " << odebrany.ilosc_produktow << "\n";
            }

            checkError( semop(sem_id, &operacjaV_ilosc_kas, 1), "Blad podniesienia semafora-3" );
            break;
        }

        if(time(NULL) - czas_startu > 10) {
            checkError( semop(sem_id, &operacjaP_nr_kasy, 1), "Blad obnizenia semafora-4" );
            int nr_kolejki_do_zmiany = kolejka->czy_oplaca_sie_zmienic_kolejke(getpid(), aktualnyNr);
            checkError( semop(sem_id, &operacjaV_nr_kasy, 1), "Blad podniesienia semafora-4" );
            if(nr_kolejki_do_zmiany != -1) {
                komunikat << "[" << "Klient-" << getpid() << "] " << "Zmienia kase z " << aktualnyNr << " do " << nr_kolejki_do_zmiany << "\n";
                checkError( semop(sem_id, &operacjaP_nr_kasy, 1), "Blad obnizenia semafora-5" );
                kolejka->usun_z_kolejki(getpid(), aktualnyNr);
                kolejka->dodaj_do_kolejki(getpid(), nr_kolejki_do_zmiany);
                checkError( semop(sem_id, &operacjaV_nr_kasy, 1), "Blad podniesienia semafora-5" );
                aktualneId = (nr_kolejki_do_zmiany == 1 ? msqid_kolejka_stac1 : msqid_kolejka_stac2);
                aktualnyNr = nr_kolejki_do_zmiany;
                czas_startu = time(NULL);
            }
        }
        sleep(1);
    }

    komunikat << "WARTOSC SEMAFORA- 1: " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";       
    checkError( semop(sem_id, &operacjaP_ilosc_klientow, 1), "Blad obnizenia semafora" );
    komunikat << "WARTOSC SEMAFORA- 2: " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";
    komunikat << "[" << "KLIENT-" << getpid() << "] " << "WYCHODZI ZE SKLEPU" << "\n" << "\n";
    
    free(kolejka);
    exit(0);
}