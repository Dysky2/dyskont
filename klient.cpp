#include <iostream>
#include <cstdlib> 
#include <ctime> 

#include "utils.h"

using namespace std;

volatile sig_atomic_t czy_klient_ma_dzialac = 1; 

void sigalrm_handler(int) { }

void opusc_sklep(int) {
    czy_klient_ma_dzialac = 0;
}

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

int main(int, char * argv[]) {
    srand(time(0) + getpid());

    signal(SIGALRM, sigalrm_handler);
    signal(SIGTERM, opusc_sklep);

    utworz_grupe_semaforowa();
    int sem_id = atoi(argv[1]);
    int shmid_kasy = atoi(argv[2]);
    int msqid_kolejka_samo = atoi(argv[3]);
    int msqid_kolejka_stac1 = atoi(argv[4]);
    int msqid_kolejka_stac2 = atoi(argv[5]);
    int shm_id_klienci = atoi(argv[6]);
    string nazwa_katalogu = argv[7];
    
    ustaw_nazwe_katalogu(nazwa_katalogu);

    StanDyskontu * stan_dyskontu = (StanDyskontu *) shmat(shmid_kasy, NULL, 0);
    DaneListyKlientow * dane_klientow = (DaneListyKlientow *) shmat(shm_id_klienci, NULL ,0);

    Kolejka * kolejka = new Kolejka(stan_dyskontu);
    ListaKlientow * lista_klientow = new ListaKlientow(dane_klientow, 0);

    operacja_v(sem_id, SEMAFOR_ILOSC_KLIENTOW);

    komunikat << "[" << "KLIENT-" << getpid() << "] " << "Wchodzi do sklepu" << "\n";
    
    // ILES CZASU JEST W SKLEPIE 
    sleep(randomTime(5));
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
    int startoweId = msqid_kolejka_samo;
    int startowyNr = 0;

    bool stac1 = stan_dyskontu->status_kasy[6] == 1;
    bool stac2 = stan_dyskontu->status_kasy[7] == 1;

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

    if(czy_klient_ma_dzialac) {

        operacja_p(sem_id, aktualnyNr);
        kolejka->dodaj_do_kolejki(getpid(), aktualnyNr);
        operacja_v(sem_id, aktualnyNr);

        komunikat << "[" << "KLIENT-" << getpid() << "] " << "Staje do kasy " << aktualnyNr << "\n";
    }

    time_t czas_startu = time(NULL);
    while(czy_klient_ma_dzialac) {
        sleep(2);
        operacja_p(sem_id, aktualnyNr);
        int czy_jestem_pierwszy = kolejka->czy_jestem_pierwszy(getpid(), aktualnyNr);
        operacja_v(sem_id, aktualnyNr);

        if(czy_jestem_pierwszy) {
            komunikat << "[KLIENT-" << getpid() << "] " << "Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[0] << "\n";
            komunikat << "[KLIENT-" << getpid() << "] " << "Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[1] << "\n";
            komunikat << "[KLIENT-" << getpid() << "] " << "Ilosc ludzi w kolejce " << stan_dyskontu->dlugosc_kolejki[2] << "\n";

            operacja_p(sem_id, SEMAFOR_ILOSC_KAS);

            Klient klient = {1 , getpid(), aktualnyNr, 0, 0, 0};
            klient.wiek = randomTimeWithRange(8, 50);
            int dlugosc_tekstu = generateProducts(&klient);

            int id_statusu = 0;
            if(aktualnyNr == 1) id_statusu = 6;
            if(aktualnyNr == 2) id_statusu = 7;

            if(stan_dyskontu->status_kasy[id_statusu] != 1) {

                operacja_p(sem_id, aktualnyNr);
                kolejka->usun_z_kolejki(getpid(), aktualnyNr);
                operacja_v(sem_id, aktualnyNr);

                operacja_p(sem_id, 0);
                kolejka->dodaj_do_kolejki(getpid(), 0);
                operacja_v(sem_id, 0);

                operacja_v(sem_id, SEMAFOR_ILOSC_KAS);

                aktualnyNr = 0;
                czas_startu = time(NULL);
                aktualneId = msqid_kolejka_samo;        

                continue;
            }

            int status = msgsnd(aktualneId, &klient, sizeof(int) * 4 + dlugosc_tekstu, 0);
            if(status != -1) {
                operacja_p(sem_id, aktualnyNr);
                kolejka->usun_z_kolejki(getpid(), aktualnyNr);
                operacja_v(sem_id, aktualnyNr);

                komunikat << "[" << "KLIENT-" << getpid() << "] " << "Ide do kasy nr: " << aktualnyNr << "\n";
            } else {
                operacja_v(sem_id, SEMAFOR_ILOSC_KAS);
                continue;
            }

            Klient odebrany;
            memset(&klient, 0, sizeof(Klient)); 
            int rcvStatus = msgrcv(aktualneId, &odebrany, sizeof(Klient) - sizeof(long int), getpid(), 0);

            if(rcvStatus == -1) {
                if(errno == EINTR) {
                    operacja_v(sem_id, SEMAFOR_ILOSC_KAS);
                    break;
                }
            } else {
                komunikat << "[" << "KLIENT-" << getpid() << "] " << "ZAKUPY DOKONANE ILOSC PRODUKTÓW " << odebrany.ilosc_produktow << "\n";
            }

            operacja_v(sem_id, SEMAFOR_ILOSC_KAS);
            break;
        }

        if(time(NULL) - czas_startu > 10) {

            operacja_p(sem_id, aktualnyNr);
            int nr_kolejki_do_zmiany = kolejka->czy_oplaca_sie_zmienic_kolejke(getpid(), aktualnyNr);
            operacja_v(sem_id, aktualnyNr);


            if(nr_kolejki_do_zmiany != -1) {

                int czy_mozna_sie_przeniesc = 0;
                int nowe_id_kolejki = msqid_kolejka_samo;

                if(nr_kolejki_do_zmiany == 1 && stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_1] == 1) {
                    czy_mozna_sie_przeniesc = 1;
                    nowe_id_kolejki = msqid_kolejka_stac1;
                } else if(nr_kolejki_do_zmiany == 2 && stan_dyskontu->status_kasy[ID_KASY_STACJONARNEJ_2] == 1) {
                    czy_mozna_sie_przeniesc = 1;
                    nowe_id_kolejki = msqid_kolejka_stac2;
                }

                if(czy_mozna_sie_przeniesc) {
                    komunikat << "[" << "KLIENT-" << getpid() << "] " << "Zmienia kase z " << aktualnyNr << " do " << nr_kolejki_do_zmiany << "\n";

                    operacja_p(sem_id, aktualnyNr);
                    kolejka->usun_z_kolejki(getpid(), aktualnyNr);
                    operacja_v(sem_id, aktualnyNr);

                    operacja_p(sem_id, nr_kolejki_do_zmiany);
                    kolejka->dodaj_do_kolejki(getpid(), nr_kolejki_do_zmiany);
                    operacja_v(sem_id, nr_kolejki_do_zmiany);

                    aktualnyNr = nr_kolejki_do_zmiany;
                    aktualneId = nowe_id_kolejki;
                    czas_startu = time(NULL);
                }
            }
        }
    }

    if(!czy_klient_ma_dzialac) {
        operacja_p(sem_id, aktualnyNr);
        kolejka->usun_z_kolejki(getpid(), aktualnyNr);
        operacja_v(sem_id, aktualnyNr);
    }

    operacja_p(sem_id, SEMAFOR_LISTA_KLIENTOW);
    lista_klientow->usun_klienta_z_listy(getpid());
    operacja_v(sem_id, SEMAFOR_LISTA_KLIENTOW);

    komunikat << "WARTOSC SEMAFORA- 1: " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";       
    operacja_p(sem_id, SEMAFOR_ILOSC_KLIENTOW);
    komunikat << "WARTOSC SEMAFORA- 2: " << semctl(sem_id, SEMAFOR_ILOSC_KLIENTOW, GETVAL) << "\n";
    komunikat << "[" << "KLIENT-" << getpid() << "] " << "WYCHODZI ZE SKLEPU" << "\n" << "\n";
    
    exit(0);
}