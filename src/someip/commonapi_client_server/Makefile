.PHONY: all server client clean

all: server client

server:
	bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd server && \
		cmake -B build -S . && \
		cmake --build build \
	"

client:
	bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd client && \
		cmake -B build -S . && \
		cmake --build build \
	"

clean:
	rm -rf server/build client/build