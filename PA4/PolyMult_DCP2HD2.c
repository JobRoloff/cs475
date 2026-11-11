// DCP2 with HHDIRECT=2 (stage 1: LL as a task, HH as an undeferred task,
// task if(0), then taskwait), as its own variant so make and run_sweep.sh
// build and gate it. All code is in PolyMult_DCP2.c.
//
// The Makefile only knows this file as the prerequisite, so an edit to
// PolyMult_DCP2.c alone does not rebuild DCP2HD2: touch this file (or
// make -B DCP2HD2) after changing PolyMult_DCP2.c.
#define HHDIRECT 2
#include "PolyMult_DCP2.c"
