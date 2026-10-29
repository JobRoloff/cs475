// DCP1 with TRED=3 and SPAWN=3 (last child run directly, then taskwait), as
// its own variant so make and run_sweep.sh build and gate it. All code is in
// PolyMult_DCP1.c.
//
// The Makefile only knows this file as the prerequisite, so an edit to
// PolyMult_DCP1.c alone does not rebuild DCP1T3S3: touch this file (or
// make -B DCP1T3S3) after changing PolyMult_DCP1.c.
#define TRED 3
#define SPAWN 3
#include "PolyMult_DCP1.c"
