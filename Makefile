main: main.o webclient.o double-conversion.o ujson.o
	g++ main.o webclient.o double-conversion.o ujson.o -o main -Wall -O3 -std=c++11 -I/usr/include/ -lcrypto -lssl

main.o:	main.cpp
	g++ -c main.cpp -Wall -O3 -std=c++11 -I/usr/include/ -I./ujson/

webclient.o: webclient.cpp
	g++ -c webclient.cpp -Wall -O3 -std=c++11 -I/usr/include/  -I./ujson/

double-conversion.o: ./ujson/double-conversion.cc
	g++ -c ./ujson/double-conversion.cc -Wall -O3 -std=c++11 

ujson.o: ./ujson/ujson.cpp
	g++ -c ./ujson/ujson.cpp -Wall -O3 -std=c++11

clean:
	rm main main.o webclient.o double-conversion.o ujson.o