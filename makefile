FILES=473_mm.h 473_mm.c

compile_1: $(FILES)
	gcc test-code1.c $(FILES) -g -o test_1