all: test

test: test.c rpa_queue.c
	gcc test.c rpa_queue.c -o test

clean:
	rm -rf test
