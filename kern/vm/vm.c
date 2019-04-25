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

    /* init the page table */
    for(i = 0; i < table_size; i++) {
        page_table[i] = NULL;
    }
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	struct addrspace *as;
	int spl;
	faultaddress &= PAGE_FRAME;

    switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		    return EFAULT;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

    if (curproc == NULL) {
		//no process
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		//no address space
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
    region curr = as->regions;
    bool notfound = true;
    mode_t dirtybit = 0;
    while (curr) {
        if ((curr->start & PAGE_FRAME) <= faultaddress && ((curr->start>>12) + curr->size)<<12 > faultaddress) {
            notfound = false;
            dirtybit = curr->ori_perms;
            break;
        }
        curr = curr->next;
    }

    // if not in address space region
    if (notfound)
        return EFAULT;
    lock_acquire(lock);
	// calculate have privillage
    dirtybit = (dirtybit & 2) ? TLBLO_DIRTY:0;
    dirtybit |= TLBLO_VALID;

    // if in hpt
    faultaddress |= as->asid;
    while (1) {
        if (page_table[hi].entryHI == faultaddress && page_table[hi].entryLO != 0) {
            spl = splhigh();
            tlb_random(page_table[hi].entryHI, page_table[hi].entryLO|dirtybit);
            splx(spl);
            lock_release(hpt_lock);
            return 0;
        } else if (page_table[hi].entryLO != 0 && page_table[hi].next != -1) {
            hi = page_table[hi].next;
        } else {
            break;
        }
    }
    // if not
    uint32_t newframe = alloc_kpages_frame();
    newframe = newframe<<12;
    if (newframe==0) {

        lock_release(hpt_lock);

        kprintf("Ran out of TLB entries - cannot handle page fault\n");

        return EFAULT;
    }

    if (hpt_insert(as, faultaddress, newframe|TLBLO_VALID)) {
        free_kpages_frame(newframe>>12);
            lock_release(hpt_lock);
            return EFAULT;
    }

    spl = splhigh();
    tlb_random(faultaddress, newframe|dirtybit);
    splx(spl);
    lock_release(hpt_lock);
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

