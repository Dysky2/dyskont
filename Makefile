all: dyskont klient kasa kasjer obsluga

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

clean:
	rm -f dyskont klient kasa kasjer obsluga *.o