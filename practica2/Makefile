EXE=miner
CC=gcc -pedantic
CFLAGS=-Wall

.PHONY: all clean
all: $(EXE)

$(EXE) : % : %.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $@.o pow.o -lpthread

%: %.o %.o
	$(CC) -p $@ $^

clean:
	rm -f *.o $(EXE) *.log