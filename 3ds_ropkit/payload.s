#ifdef EUR_BUILD
#include "parameters_eur.s"
#include "addr_eur.s"
#endif
#ifdef USA_BUILD
#include "parameters_usa.s"
#include "addr_usa.s"
#endif
#ifdef JAP_BUILD
#include "parameters_jap.s"
#include "addr_jap.s"
#endif
#include "ropkit_ropinclude.s"
_start:
ropstackstart:
#include "ropkit_boototherapp.s"
ropkit_cmpobject:
.word (ROPBUFLOC(ropkit_cmpobject) + 0x4) @ Vtable-ptr
.fill (0x40 / 4), 4, STACKPIVOT_ADR @ Vtable