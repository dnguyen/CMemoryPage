FILES=473_mm.h 473_mm.c

compile_1: $(FILES)
	gcc test-code1.c $(FILES) -g -o test_1

compile_2: $(FILES)
	gcc test-code2.c $(FILES) -g -o test_2

compile_3: $(FILES)
	gcc test-code3.c $(FILES) -g -o test_3

compile_4: $(FILES)
	gcc test-code4.c $(FILES) -g -o test_4

compile_5: $(FILES)
	gcc test-code5.c $(FILES) -g -o test_5

compile_6: $(FILES)
	gcc test-code6.c $(FILES) -g -o test_6