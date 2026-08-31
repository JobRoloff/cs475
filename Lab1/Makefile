CFLAGS  = -O2 -std=c99
LIBRARIES = -lm
CC = gcc

all: bin bin/Mandelbrot.time bin/Mandelbrot.time-par bin/Mandelbrot.check-par

debug: CFLAGS = -DDEBUG -g -Wall -Wextra -std=c99
debug: all

# TASK BINARIES
#
# Note: there is deliberately no "Mandelbrot.check" (sequential Current vs.
# Gold) target. Gold is compiled from this same Mandelbrot.c (see -DGOLD
# below), so a sequential-vs-Gold check would just be the same code checked
# against itself -- always 0 errors, tells you nothing. Mandelbrot.check-par
# is the one correctness check that's actually meaningful: it verifies your
# parallel version against the trusted sequential reference.
#
# Every target below also depends on Makefile itself, so editing CFLAGS,
# a build rule, etc. triggers a rebuild -- not just editing the .c files.

bin/Mandelbrot.time: src/Mandelbrot-wrapper.c bin/Mandelbrot.o Makefile
	$(CC) src/Mandelbrot-wrapper.c -o $@ bin/Mandelbrot.o $(CFLAGS) $(LIBRARIES)

bin/Mandelbrot.time-par: src/Mandelbrot-wrapper.c bin/MandelbrotPAR.o Makefile
	$(CC) src/Mandelbrot-wrapper.c -o $@ bin/MandelbrotPAR.o $(CFLAGS) $(LIBRARIES) -fopenmp -DPAR

bin/Mandelbrot.check-par: src/Mandelbrot-wrapper.c bin/MandelbrotPAR.o bin/MandelbrotGold.o Makefile
	$(CC) src/Mandelbrot-wrapper.c -o $@ bin/MandelbrotPAR.o bin/MandelbrotGold.o $(CFLAGS) $(LIBRARIES) -fopenmp -DPAR -DCHECKING

# OBJECT FILES
#
# All three come from the same src/Mandelbrot.c -- no separate Gold source.

bin/Mandelbrot.o: src/Mandelbrot.c Makefile
	$(CC) src/Mandelbrot.c -o bin/Mandelbrot.o $(CFLAGS) $(LIBRARIES) -c

bin/MandelbrotPAR.o: src/Mandelbrot.c Makefile
	$(CC) src/Mandelbrot.c -o bin/MandelbrotPAR.o -fopenmp -DPAR $(CFLAGS) $(LIBRARIES) -c

bin/MandelbrotGold.o: src/Mandelbrot.c Makefile
	$(CC) src/Mandelbrot.c -o bin/MandelbrotGold.o -DGOLD $(CFLAGS) $(LIBRARIES) -c

# MISC

bin:
	mkdir -p bin

clean:
	rm -f bin/Mandelbrot.time bin/Mandelbrot.time-par bin/Mandelbrot.check-par bin/*.o
