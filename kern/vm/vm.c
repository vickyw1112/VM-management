#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>

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
	char perms;
    struct addrspace *as;
	struct entry *pe = NULL;
	if(faultaddress == 0x0 || faultaddress >= 0x80000000)
		return EFAULT;
    

	as = proc_getas();
	if(as == NULL){
		return EFAULT;
	}
	pe = pt_search(as, faultaddress);
	switch (faulttype) {
        case VM_FAULT_READ:
   	    case VM_FAULT_WRITE:
			if(!pe){
				uint32_t newframe = alloc_kpages(1);
				newframe =newframe >> 12;

				if(newframe == 0){
					return EFAULT; // tlb out of entries - cannot handle
				}
				region_perm_search(as, faultaddress, &perms);

				pt_insert(as, KVADDR_TO_PADDR(newframe), perms);

				tlb_random(newframe, KVADDR_TO_PADDR(newframe));
			}		
			break;
		case VM_FAULT_READONLY:

			break;
		}
	as_activate();
	return 0;
	
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

