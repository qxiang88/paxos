all: master server client

master: master.o master-socket.o
	g++ -g -std=c++0x -o master master.o master-socket.o

master.o: master.cpp master.h constants.h
	g++ -g -std=c++0x -c master.cpp

master-socket.o: master-socket.cpp master.h
	g++ -g -std=c++0x -c master-socket.cpp

server: server.o server-socket.o
	g++ -g -std=c++0x -o server server.o server-socket.o -pthread

server.o: server.cpp server.h constants.h
	g++ -g -std=c++0x -c server.cpp

server-socket.o: server-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c server-socket.cpp

client: client.o client-socket.o
	g++ -g -std=c++0x -o client client.o client-socket.o -pthread

client.o: client.cpp client.h constants.h
	g++ -g -std=c++0x -c client.cpp

client-socket.o: client-socket.cpp client.h constants.h
	g++ -g -std=c++0x -c client-socket.cpp

clean:
	rm -f *.o master server client