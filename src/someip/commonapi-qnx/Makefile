.PHONY: all download patch build clean

all: build

build: patch
	./scripts/build.sh

patch: download
	./scripts/patch.sh

download:
	./scripts/download.sh

clean:
	rm -rf build-rpi third_party