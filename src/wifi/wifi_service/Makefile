CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -O2 -Wall
LDFLAGS = -lsocket
TARGET = wifi_service

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
