all: master server client

# master related
master: master.o master-socket.o utilities.o
	g++ -g -std=c++0x -o master master.o utilities.o master-socket.o -pthread

master.o: master.cpp master.h constants.h
	g++ -g -std=c++0x -c master.cpp

master-socket.o: master-socket.cpp master.h
	g++ -g -std=c++0x -c master-socket.cpp

# server related
server: server.o server-socket.o replica.o replica-socket.o \
		leader.o leader-socket.o acceptor.o acceptor-socket.o \
		commander.o commander-socket.o scout.o scout-socket.o utilities.o
	g++ -g -std=c++0x -o server server.o server-socket.o \
		replica.o replica-socket.o leader.o leader-socket.o \
		acceptor.o acceptor-socket.o commander.o commander-socket.o \
		scout.o scout-socket.o utilities.o -pthread

server.o: server.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c server.cpp

server-socket.o: server-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c server-socket.cpp

replica.o: replica.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c replica.cpp

replica-socket.o: replica-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c replica-socket.cpp

leader.o: leader.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c leader.cpp

leader-socket.o: leader-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c leader-socket.cpp

acceptor.o: acceptor.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c acceptor.cpp

acceptor-socket.o: acceptor-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c acceptor-socket.cpp

commander.o: commander.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c commander.cpp

commander-socket.o: commander-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c commander-socket.cpp

scout.o: scout.cpp server.h constants.h utilities.h
	g++ -g -std=c++0x -c scout.cpp

scout-socket.o: scout-socket.cpp server.h constants.h
	g++ -g -std=c++0x -c scout-socket.cpp


#client related
client: client.o client-socket.o utilities.o
	g++ -g -std=c++0x -o client client.o client-socket.o utilities.o -pthread

client.o: client.cpp client.h constants.h utilities.h
	g++ -g -std=c++0x -c client.cpp

client-socket.o: client-socket.cpp client.h constants.h
	g++ -g -std=c++0x -c client-socket.cpp

#general
utilities.o: utilities.cpp utilities.h constants.h
	g++ -g -std=c++0x -c utilities.cpp

clean:
	rm -f *.o master server client