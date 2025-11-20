CC = gcc
CFLAGS = -pthread
TARGET = jsmon
SRC = jsmon.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean