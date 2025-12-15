all: dyskont klient kasa kasjer obsluga kierownik

dyskont: dyskont.cpp utils.h
	g++ dyskont.cpp -o dyskont

klient: klient.cpp utils.h
	g++ klient.cpp -o klient

kasa: kasa.cpp utils.h
	g++ kasa.cpp -o kasa

kasjer: kasjer.cpp utils.h
	g++ kasjer.cpp -o kasjer

obsluga: obsluga.cpp utils.h
	g++ obsluga.cpp -o obsluga

kierownik: kierownik.cpp utils.h
	g++ kierownik.cpp -o kierownik

clean:
	rm -f dyskont klient kasa kasjer obsluga *.o