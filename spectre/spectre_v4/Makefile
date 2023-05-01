CFLAGS += -pie -std=gnu99 -lpthread -fPIC

PROGRAM = spectre
SOURCE  = spectre.c
     
all: $(PROGRAM)

$(PROGRAM): $(SOURCE)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(SOURCE)

clean:
	rm -f $(PROGRAM)
