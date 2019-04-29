#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <spl.h>
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
	int spl;
    struct addrspace *as;
	struct entry *pe = NULL;
	uint32_t entrylo, entryhi = faultaddress & TLBHI_VPAGE;
	if(faultaddress == 0x0 || faultaddress >= 0x80000000)
		return EFAULT;
    

	as = proc_getas();
	if(as == NULL){
		return ENOMEM;
	}

	spl = splhigh();
	if(faulttype == VM_FAULT_READ || faulttype == VM_FAULT_WRITE){

		pe = pt_search(as, faultaddress);
		if(!pe){
			// check whether faultaddress is in a region
			perms = region_perm_search(as, faultaddress);
			if(perms == -1)
				return EFAULT;
			
			// alloc new frame
			uint32_t newframe = alloc_kpages(1);
			bzero((void *)newframe, PAGE_SIZE);
			if(newframe == 0x0){
				return ENOMEM; // tlb out of entries - cannot handle
			}

			pe = pt_insert(as, KVADDR_TO_PADDR(newframe), faultaddress, perms);
		}
		entrylo = pe->entrylo;


		if(pe->permissions & WRITE){
			entrylo |= TLBLO_DIRTY;
		}else{
			entrylo |= as->isLoading ? TLBLO_DIRTY : 0;
		}

        entrylo |= pe->permissions ? TLBLO_VALID : 0; /* set valid bit */ 

		tlb_random(entryhi, entrylo);
		splx(spl);
		return 0;
	} 
	else if(faultaddress == VM_FAULT_READONLY){
		return EPERM;
	}	
	return EINVAL;
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

