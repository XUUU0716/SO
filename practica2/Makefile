EXE=miner
CC=gcc -pedantic
CFLAGS=-Wall

.PHONY: all clean test valgrind
all: $(EXE)

$(EXE) : % : %.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $@.o pow.o -lpthread

%: %.o %.o
	$(CC) -p $@ $^

test:
	./miner 10 8 & ./miner 10 8 & ./miner 10 8 & ./miner 10 1

valgrind:
 	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./miner 10 4

clean:
	rm -f *.o $(EXE) *.txt *.tgt *.pid
	
clean_semaforos:
	rm -f /dev/shm/sem.miner_*