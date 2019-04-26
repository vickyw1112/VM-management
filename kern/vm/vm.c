#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */

void vm_bootstrap(void)
{
	/* Initialise VM sub-system.  You probably want to initialise your 
	   frame table here as well.
	*/

}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	int spl, perms, region;
    struct addrspace *as;
	if(faultaddress == 0x0 || faultaddress >= 0x80000000)
		return EFAULT;
    
	as = proc_getas();
	struct entry *pe = pt_search(as, faultaddress);

	switch (faulttype) {
        case VM_FAULT_READ:
   	    case VM_FAULT_WRITE:

		   break;
        case VM_FAULT_READONLY:

			break;
	return EFAULT;
}





/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

