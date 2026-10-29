// DCP1 with ROOTDIRECT=1 (single thread runs the root itself), as its own variant so make and run_sweep.sh build and
// gate it. All code is in PolyMult_DCP1.c.
//
// The Makefile only knows this file as the prerequisite, so an edit to
// PolyMult_DCP1.c alone does not rebuild DCP1RD: touch this file (or
// make -B DCP1RD) after changing PolyMult_DCP1.c.
#define ROOTDIRECT 1
#include "PolyMult_DCP1.c"
