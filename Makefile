CC := gcc
CFLAGS := -Wall -g -Isrc

SRC_DIR := src
BUILD_DIR := dist

CLIENT_SOURCE := client.c $(SRC_DIR)/clientFunctions.c $(SRC_DIR)/socket.c $(SRC_DIR)/game.c $(SRC_DIR)/receiveBuffer.c
SERVER_SOURCE := server.c $(SRC_DIR)/serverFunctions.c $(SRC_DIR)/socket.c $(SRC_DIR)/game.c $(SRC_DIR)/receiveBuffer.c

CLIENT_OBJ := $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(CLIENT_SOURCE)))
SERVER_OBJ := $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SERVER_SOURCE)))

TARGET_CLIENT := client
TARGET_SERVER := server

all: $(TARGET_CLIENT) $(TARGET_SERVER)

# Link client
$(TARGET_CLIENT): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

# Link server
$(TARGET_SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

# Compile source files to dist/ folder
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure dist/ exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean
