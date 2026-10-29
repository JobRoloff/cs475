// DCP1 with ROOTDIRECT=1 and SPAWN=3 (root run directly, last child run directly, taskwait), as its own variant so make and run_sweep.sh build and
// gate it. All code is in PolyMult_DCP1.c.
//
// The Makefile only knows this file as the prerequisite, so an edit to
// PolyMult_DCP1.c alone does not rebuild DCP1RDS3: touch this file (or
// make -B DCP1RDS3) after changing PolyMult_DCP1.c.
#define ROOTDIRECT 1
#define SPAWN 3
#include "PolyMult_DCP1.c"
