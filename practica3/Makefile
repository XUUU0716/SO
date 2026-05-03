EXE=miner monitor
CC=gcc -pedantic
CFLAGS=-Wall
LIBS=-lrt -pthread

.PHONY: all clean
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

clean:
	rm -f *.o $(EXE) *.log *.txt *.pid *.tgt
	rm -f /dev/shm/sem.miner_* /dev/shm/sem.sem_monitor_* /dev/shm/shm_monitor
	rm -f /dev/mqueue/mq_monitor