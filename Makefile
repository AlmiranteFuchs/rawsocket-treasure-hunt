CC := gcc
CFLAGS := -Wall 

CLIENT_SOURCE := client.c clientFunctions.c socket.c
CLIENT_OBJ := client.o clientFunctions.o socket.o

SERVER_SOURCE := server.c serverFunctions.c socket.c
SERVER_OBJ := server.o serverFunctions.o socket.o

TARGET_CLIENT := client
TARGET_SERVER := server

all: $(TARGET_CLIENT) $(TARGET_SERVER)

$(TARGET_CLIENT): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean
