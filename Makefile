CFLAGS=-Wall -Wextra -Werror -O2
TARGETS= lab1_test lab1 lib.so

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -rf *.o $(TARGETS)

lab1_test: lab1_test.c plugin_api.h
	gcc $(CFLAGS) -o lab1_test lab1_test.c -ldl

lab1: lab1.c plugin_api.h
	gcc $(CFLAGS) -o lab1 lab1.c -ldl

lib.so: lib.c plugin_api.h
	gcc $(CFLAGS) -shared -fPIC -o lib.so lib.c -ldl -lm
