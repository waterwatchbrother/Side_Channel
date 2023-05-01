CFLAGS += -g -std=gnu99 -O2 -pthread -Wall

PROGRAM = meltdown-3a
SOURCE = source.c sregs.S 

all: $(PROGRAM)

$(PROGRAM): $(SOURCE)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(SOURCE)

clean:
	rm -f $(PROGRAM)
