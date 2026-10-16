CC = gcc
OBJS = timer.o
FLAGS = -O3 -fopenmp
DEBUGFLAG = -DDEBUG
SEQFLAGS = -O3 -lm

EXEC = Fib_count Fib_count_debug Fib_count_race Fib_count_atomic Fib_count_red Fib_count_red_cutoff Fib_count_atomic_cutoff

all: $(EXEC)

Fib_count: Fib_count.c $(OBJS)
	$(CC) $(SEQFLAGS)  -o $@ $< $(OBJS)

Fib_count_race: Fib_count_race.c $(OBJS)
	$(CC) $(FLAGS)  -o $@ $< $(OBJS)

Fib_count_atomic: Fib_count_atomic.c $(OBJS)
	$(CC) $(FLAGS)  -o $@ $< $(OBJS)

Fib_count_red: Fib_count_red.c $(OBJS)
	$(CC) $(FLAGS)  -o $@ $< $(OBJS)

Fib_count_red_cutoff: Fib_count_red_cutoff.c $(OBJS)
	$(CC) $(FLAGS)  -o $@ $< $(OBJS)

Fib_count_atomic_cutoff: Fib_count_atomic_cutoff.c $(OBJS)
	$(CC) $(FLAGS)  -o $@ $< $(OBJS)

Fib_count_debug:Fib_count.c $(OBJS)
	$(CC) $(SEQFLAGS) $(DEBUGFLAG)  -o $@ $< $(OBJS)

timer.o: timer.c
	$(CC) -O3 -o $@ -c $<

clean:
	rm -f $(OBJS) $(EXEC)
