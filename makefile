all: master server client

master: master.o socket.o
	g++ -g -std=c++0x -o master master.o socket.o

master.o: master.cpp master.h constants.h
	g++ -g -std=c++0x -c master.cpp

server: server.o
	g++ -g -std=c++0x -o server server.o -pthread

server.o: server.cpp server.h constants.h
	g++ -g -std=c++0x -c server.cpp

client: client.o
	g++ -g -std=c++0x -o client client.o -pthread

client.o: client.cpp client.h constants.h
	g++ -g -std=c++0x -c client.cpp

socket.o: socket.cpp master.h
	g++ -g -std=c++0x -c socket.cpp
clean:
	rm -f *.o master server client