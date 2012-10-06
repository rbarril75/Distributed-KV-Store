SYSTEM_EXE = mp3
CLIENT_EXE = mp3api

SYSTEM_OBJS = failuredetector.o socket.o kvstore.o mp3.o
CLIENT_OBJS = socket.o mp3API.o

COMPILER = g++
COMPILER_OPTS = -c -g -Wall

LINKER = g++
LINKER_OPTS = -o

all: $(SYSTEM_EXE) $(CLIENT_EXE)

$(SYSTEM_EXE) : $(SYSTEM_OBJS)
	$(LINKER) $(LINKER_OPTS) $(SYSTEM_EXE) $(SYSTEM_OBJS)

$(CLIENT_EXE) : $(CLIENT_OBJS)
	$(LINKER) $(LINKER_OPTS) $(CLIENT_EXE) $(CLIENT_OBJS)
	
socket.o : socket.cpp socket.h
	$(COMPILER) $(COMPILER_OPTS) socket.cpp

failuredetector.o : failuredetector.cpp failuredetector.h
	$(COMPILER) $(COMPILER_OPTS) failuredetector.cpp

kvstore.o : kvstore.cpp kvstore.h
	$(COMPILER) $(COMPILER_OPTS) kvstore.cpp

mp3.o : mp3.cpp failuredetector.h kvstore.h
	$(COMPILER) $(COMPILER_OPTS) mp3.cpp
	
mp3API.o : mp3API.cpp socket.h
	$(COMPILER) $(COMPILER_OPTS) mp3API.cpp

clean:
	-rm *.o mp3 mp3api
