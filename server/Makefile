CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -Werror

all: default

default: aesdsocket

aesdsocket: aesdsocket.o

clean:
	rm -f aesdsocket aesdsocket.o

.PHONY: all default clean
