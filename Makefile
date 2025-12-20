CFLAGS ?= -Wall -Wextra -O2

TARGETS = dyskont klient kasa kasjer obsluga kierownik

UTILS_SRC = utils.cpp

all: $(TARGETS)

$(TARGETS): %: %.cpp $(UTILS_SRC)
	g++ $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGETS) *.o