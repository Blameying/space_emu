
test: test_iomap.o iomap.o memory.o regs.o test.c
	gcc -g $^ -o test

memory.o: memory.c
	gcc -c $^ -I./ -o $@

regs.o: regs.c
	gcc -c $^ -I./ -o $@

test_iomap.o: test_iomap.c
	gcc -c $^ -I./ -o $@

iomap.o: iomap.c
	gcc -c $^ -I./ -o $@

clean:
	rm *.o test
