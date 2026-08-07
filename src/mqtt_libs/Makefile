SHELL := /bin/bash
SCRIPT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all download compile install clean

all: install

download:
	cd $(SCRIPT_DIR) && ./download_mosquitto.sh

compile:
	cd $(SCRIPT_DIR) && ./cross_compile_qnx.sh

install: download compile

clean:
	rm -rf $(SCRIPT_DIR)/build_qnx $(SCRIPT_DIR)/install_qnx