CC   = gcc
STD  = -std=gnu11
OPT  = -O3 -march=native
LIBS = -lm

HARNESS  = shared/harness
INCLUDE  = -Ishared/include -I$(HARNESS)

CHECK_H = $(HARNESS)/oracle.c $(HARNESS)/check_main.c
TIME_H  = $(HARNESS)/range.c $(HARNESS)/args.c $(HARNESS)/time_main.c

# Listed as prerequisites so editing a header actually triggers a rebuild.
# Without these, a change to accumulate.h leaves stale binaries in place
# and the correctness gate silently re-runs the old code.
HEADERS = shared/include/kernel.h shared/include/accumulate.h \
          $(HARNESS)/args.h $(HARNESS)/range.h $(HARNESS)/oracle.h

# Every PolyMult_<V>.c in this directory is a variant; adding a file adds a
# variant, no edit here needed.
VARIANTS := $(patsubst PolyMult_%.c,%,$(wildcard PolyMult_*.c))
GOLDED   := $(filter-out GSQ,$(VARIANTS))

# Gold suite: non-GSQ check binaries also compare against GSQ. gold.o is
# provided precompiled and is never rebuilt here. GSQ's source is recompiled
# with kernel_multiply renamed to bpm_gold_multiply, so both kernels can live
# in one binary. That recompile deliberately takes neither -DBPM_VARIANT nor
# -DPAR: it must stay GSQ's true checking build. GSQ itself links no gold --
# checking it against a copy of itself would prove nothing.
GSQ_SRC   = PolyMult_GSQ.c
GOLD      = -DBPM_HAVE_GOLD
GOLD_OBJS = $(HARNESS)/gold.o bin/gsq_gold.o

.PHONY: all debug clean $(VARIANTS)

all: $(VARIANTS)

# `make BKQ` builds just that variant's four binaries.
$(VARIANTS): %: bin/PolyMult_%.check bin/PolyMult_%.check-par bin/PolyMult_%.time bin/PolyMult_%.time-par

bin:
	mkdir -p bin

bin/gsq_gold.o: $(GSQ_SRC) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DCHECKING -Dkernel_multiply=bpm_gold_multiply -c $(GSQ_SRC) -o $@

bin/PolyMult_%.time: PolyMult_%.c $(TIME_H) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"$*"' -ffast-math $< $(TIME_H) -o $@ $(LIBS)

bin/PolyMult_%.time-par: PolyMult_%.c $(TIME_H) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"$*"' -ffast-math -DPAR -fopenmp $< $(TIME_H) -o $@ $(LIBS)

$(GOLDED:%=bin/PolyMult_%.check): bin/PolyMult_%.check: PolyMult_%.c $(CHECK_H) $(GOLD_OBJS) $(HEADERS) $(HARNESS)/gold.h | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"$*"' $(GOLD) -DCHECKING $< $(CHECK_H) $(GOLD_OBJS) -o $@ $(LIBS)

$(GOLDED:%=bin/PolyMult_%.check-par): bin/PolyMult_%.check-par: PolyMult_%.c $(CHECK_H) $(GOLD_OBJS) $(HEADERS) $(HARNESS)/gold.h | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"$*"' $(GOLD) -DCHECKING -DPAR -fopenmp $< $(CHECK_H) $(GOLD_OBJS) -o $@ $(LIBS)

bin/PolyMult_GSQ.check: PolyMult_GSQ.c $(CHECK_H) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"GSQ"' -DCHECKING $< $(CHECK_H) -o $@ $(LIBS)

bin/PolyMult_GSQ.check-par: PolyMult_GSQ.c $(CHECK_H) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) -DBPM_VARIANT='"GSQ"' -DCHECKING -DPAR -fopenmp $< $(CHECK_H) -o $@ $(LIBS)

debug: OPT = -O0 -g -Wall -Wextra
debug: all

clean:
	rm -f bin/*
