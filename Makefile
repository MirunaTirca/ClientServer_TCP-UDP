# Protocoale de comunicatii:
# Makefile

CFLAGS = -Wall -g

# Portul pe care asculta serverul (de completat)
PORT = $3

# Adresa IP a serverului (de completat)
IP_SERVER = $2 

#ID client
ID_CLIENT = $1

all: server subscriber

# Compileaza server.c
server: server.c

# Compileaza subscriber.c
client: -g subscriber.c

.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriber
run_client:
	./subscriber $(ID_CLIENT) ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
