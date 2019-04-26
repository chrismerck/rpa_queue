all: test

test: test.c rpa_queue.c
	gcc test.c rpa_queue.c -g -o test

clean:
	rm -rf test
