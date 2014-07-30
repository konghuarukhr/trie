CC=g++
CFLAG=-O2 -DNDEBUG -Wall
LDFLAG=-O2 -Wall

ALL: trie-test

trie-test: trie-test.o trie.o
	$(CC) $(LDFLAG) trie-test.o trie.o -o trie-test

trie-test.o: trie-test.cpp trie.h
	$(CC) $(CFLAG) -c trie-test.cpp

trie.o: trie.cpp trie.h
	$(CC) $(CFLAG) -c trie.cpp

clean:
	rm -f trie-test *.o
