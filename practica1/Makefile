EXE=miner
CC=gcc -pedantic
CFLAGS=-Wall

.PHONY: clean

$(EXE) : % : %.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $@.o pow.o

%: %.o %.o
	$(CC) -p $@ $^

clean:
	rm -f *.o $(EXE) *.log