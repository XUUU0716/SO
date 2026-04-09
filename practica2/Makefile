EXE=miner
CC=gcc -pedantic 
CFLAGS=-Wall

.PHONY: all clean test valgrind clean_semaforos

all: $(EXE)

$(EXE): miner.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depends on $^"
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	./miner 30 8 & ./miner 30 8 & ./miner 30 8 & ./miner 30 1 

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file="val_log_%p.txt" ./miner 5 4 & \
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file="val_log_%p.txt" ./miner 8 2 & \
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file="val_log_%p.txt" ./miner 10 2 

clean:
	rm -f *.o $(EXE) *.txt *.tgt *.pid
	
clean_run:
	rm -f *.txt *.tgt *.pid /dev/shm/sem.miner_*

clean_semaforos:
	rm -f /dev/shm/sem.miner_*