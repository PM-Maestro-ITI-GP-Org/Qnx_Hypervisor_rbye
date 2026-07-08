.PHONY: all commonapi-qnx commonapi_client_server clean

all: commonapi_client_server

commonapi-qnx:
	$(MAKE) -C commonapi-qnx

commonapi_client_server: commonapi-qnx
	$(MAKE) -C commonapi_client_server

clean:
	$(MAKE) -C commonapi_client_server clean
	$(MAKE) -C commonapi-qnx clean