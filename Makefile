
target: test_encode

test_encode: test_encode.c encode.c
	gcc -Wall -o $@ $^ -lz -lm
