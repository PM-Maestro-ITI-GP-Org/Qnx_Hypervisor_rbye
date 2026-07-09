CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall
TARGET = spi_loopback

all: $(TARGET)

$(TARGET): spi_loopback.c
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
