CFLAGS += -pie -std=gnu99 -lpthread -fPIC

PROGRAM = flush+reload
SOURCE  = source.c
     
all: $(PROGRAM)

$(PROGRAM): $(SOURCE)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(SOURCE)

clean:
	rm -f $(PROGRAM)
