# Makefile für key2joy

CC      := gcc
CFLAGS  := -pthread -O3
TARGET  := key2joy
SRC     := key2joy.c

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod 755 /usr/local/bin/$(TARGET)
	@echo "✅ key2joy wurde nach /usr/local/bin installiert."

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "🗑️  key2joy wurde entfernt."

clean:
	rm -f $(TARGET)
