dyskont: dyskont.cpp klient.o kasa.o kasjer.o obsluga.o
	g++ dyskont.cpp -o dyskont

klient.o: klient.cpp utils.h
	g++ klient.cpp -o klient

kasa.o: kasa.cpp
	g++ kasa.cpp -o kasa

kasjer.o: kasjer.cpp
	g++ kasjer.cpp -o kasjer

obsluga.o: obsluga.cpp
	g++ obsluga.cpp -o obsluga