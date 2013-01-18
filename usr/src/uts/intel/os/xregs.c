#include <sys/klwp.h>
#include <sys/ucontext.h>
#include <sys/procfs.h>


/*
 * Indicate whether or not an extra register state
 * pointer is associated with a struct ucontext.
 */
int
xregs_hasptr(klwp_id_t lwp, ucontext_t *uc)
{
	_NOTE();

	return (uc->uc_mcontext.xrs.xrs_id == XRS_ID);
}

/*
 * Get the struct ucontext extra register state pointer field.
 */
caddr_t
xregs_getptr(klwp_id_t lwp, ucontext_t *uc)
{
	_NOTE();

	if (uc->uc_mcontext.xrs.xrs_id == XRS_ID)
		return (uc->uc_mcontext.xrs.xrs_ptr);

	return (NULL);
}

void
xregs_setptr(klwp_id_t lwp, ucontext_t *uc, caddr_t xrp)
{
	uc->uc_mcontext.xrs.xrs_id = XRS_ID;
	uc->uc_mcontext.xrs.xrs_ptr = xrp;
}
