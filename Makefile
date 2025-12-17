CFLAGS ?= -Wall -Wextra -O2

all: dyskont klient kasa kasjer obsluga kierownik

dyskont: dyskont.cpp utils.h
	g++ $(CFLAGS) dyskont.cpp -o dyskont

klient: klient.cpp utils.h
	g++ $(CFLAGS) klient.cpp -o klient

kasa: kasa.cpp utils.h
	g++ $(CFLAGS) kasa.cpp -o kasa

kasjer: kasjer.cpp utils.h
	g++ $(CFLAGS) kasjer.cpp -o kasjer

obsluga: obsluga.cpp utils.h
	g++ $(CFLAGS) obsluga.cpp -o obsluga

kierownik: kierownik.cpp utils.h
	g++ $(CFLAGS) kierownik.cpp -o kierownik

clean:
	rm -f dyskont klient kasa kasjer obsluga *.o