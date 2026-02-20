CC = g++
CFLAGS = -Wall -std=c++11
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: server_app client_app

server_app: server/main.cpp
	$(CC) $(CFLAGS) server/main.cpp -o server_app

client_app: client/main.cpp
	$(CC) $(CFLAGS) client/main.cpp -o client_app $(RAYLIB_FLAGS)

clean:
	rm -f server_app client_app