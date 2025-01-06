all: mgse vmextract slicer

mgse: parser.o mg-symengine.o
	g++ -std=c++17 -Wall -Wextra -pedantic -g main.cpp parser.o mg-symengine.o -o mgse

vmextract: parser.o
	g++ -std=c++17 -Wall -Wextra -pedantic -g vmextract.cpp parser.o -o vmextract

slicer: core.o parser.o
	g++ -std=c++17 -Wall -Wextra -pedantic -g slicer.cpp core.o parser.o -o slicer

core.o:
	g++ -c -std=c++17 -Wall -Wextra -pedantic -g core.cpp

parser.o:
	g++ -c -std=c++17 -Wall -Wextra -pedantic -g parser.cpp parser.hpp

mg-symengine.o:
	g++ -c -std=c++17 -Wall -Wextra -pedantic -g mg-symengine.cpp

clean:
	rm -f core.o parser.o mg-symengine.o mgse slicer vmextract
