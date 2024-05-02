CC=	    gcc
CFLAGS=	    -Wall -std=gnu99 -pthread

jobsched : jobsched.c
	$(CC) $(CFLAGS) $< -o $@
	
test : jobsched 
	./jobsched < test.txt

all: jobsched
clean:
	rm jobsched
	rm *.wav