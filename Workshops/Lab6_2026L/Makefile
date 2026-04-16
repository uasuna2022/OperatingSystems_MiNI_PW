override CFLAGS=-std=c2x -Wall -Wextra -Wshadow -Wvla -Wno-unused-parameter -Wno-unused-const-variable -g -O0 -fsanitize=address,undefined,leak

ifdef CI
override CFLAGS=-std=c2x -Wall -Wextra -Wshadow -Wvla -Werror -Wno-unused-parameter -Wno-unused-const-variable
endif

NAME=sop-keys

.PHONY: all clean

all: ${NAME}

${NAME}: ${NAME}.c
	$(CC) $(CFLAGS) -pthread -o ${NAME} ${NAME}.c

clean:
	rm -f ${NAME}
