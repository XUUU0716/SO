EXE=miner monitor
CC=gcc -pedantic
CFLAGS=-Wall -g
LIBS=-lrt -pthread

.PHONY: all clean miner monitor valgrind_monitor valgrind_minero
all: $(EXE)

miner: miner.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

monitor: monitor.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

miner.o: miner.c shared_data.h pow.h
monitor.o: monitor.c shared_data.h pow.h
pow.o: pow.c pow.h

valgrind_monitor:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes ./monitor 500 500

valgrind_minero:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes ./miner 15 4
	
clean:
	rm -f *.o $(EXE) *.log *.txt *.pid *.tgt
	rm -f /dev/shm/sem.miner_* /dev/shm/sem.sem_monitor_* /dev/shm/shm_monitor
	rm -f /dev/mqueue/mq_monitor