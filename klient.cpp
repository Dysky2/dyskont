#include <iostream>
#include <cstdlib> 
#include <ctime> 
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

using namespace std;

int randomTimeWithRange(int min, int max) {
    return rand() % (max - min + 1) + min; 
}

int randomTime(int time) {
    return rand() % time; 
}

void generateProducts() {
    const string kategorieProduktow[10] = {
        "OWOCE",
        "WARZYWA",
        "PIECZYWO",
        "NABIAL",
        "ALHOHOL",
        "WEDLINY",
        "MAKARONY",
        "NAPOJE",
        "MIESO",
        "RYBY"
    };

    const vector<vector<string>> products = {
        {"Jablko", "Banan", "Gruszka", "Truskawka", "Arbuz"},
        {"Marchew", "Ziemniak", "Pomidor", "Ogorek" },
        {"Chleb Razowy", "Bulka Kajzerka", "Bagietka"},
        {"Mleko", "Ser Zolty", "Jogurt Naturalny", "Maslo", "Smietana", "Kefir" },
        {"Piwo", "Wino", "Wodka", "Whisky"},
        {"Szynka Babuni", "Kielbasa Slaska", "Kabanosy", "Parowki", "Salami" },
        {"Spaghetti", "Swiderki", "Penne"},
        { "Woda Niegazowana", "Cola", "Sok Pomaranczowy", "Zielona Herbata"},
        {"Piers z Kurczaka", "Schab", "Wolowina", "Mielone"},
        { "Losos", "Dorsz", "Pstrag"}
    };

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

    struct sembuf operacjaV = {0, -1, SEM_UNDO};

    int time = randomTime(10);

    cout << "Klient wchodzi do sklepu" << endl;

    // ILES CZASU JEST W SKLEPIE 
    sleep(time);
    // ten sleep powinien byc git po jakby wstrzymuje ten proces na iles
    
    generateProducts();

    cout << "\nKlient wychodzi po " << time << " sekund" << endl;

    semop(atoi(argv[1]), &operacjaV, 1);

    exit(0);
}