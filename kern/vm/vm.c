#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */
bool pt_insert(struct addrspace *as, vaddr_t hi, paddr_t lo) {
    uint32_t newprev = as.regions->start
    if (pt[newprev].entryLO == 0) {
        pt[newprev].entryHI = hi;
        pt[newprev].entryLO = lo;
        pt[newprev].as = as;
        return false;
    }
    while (pt[newprev].next != -1) {
        newprev = pt[newprev].next;
    }
    for (uint32_t newindex = 0; newindex < table_size; ++newindex) {
        if (pt[newindex].entryHI==0 && pt[newindex].entryLO==0 && pt[newindex].next==-1) {
            pt[newindex].entryHI = hi;
            pt[newindex].entryLO = lo;
            pt[newindex].as = as;
            pt[newprev].next = newindex;
            return false;
        }
    }
    return true;
}

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
	lock = lock_create("lock);
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

    if (notfound)
        return EFAULT;
    lock_acquire(lock); //need a lock code region
	// calculate have privillage
    dirtybit = (dirtybit & 2) ? TLBLO_DIRTY:0;
    dirtybit |= TLBLO_VALID;
	
    faultaddress |= as->asid;
    while (1) {
        if (page_table[hi].entryHI == faultaddress && page_table[hi].entryLO != 0) {
            spl = splhigh();
            tlb_random(page_table[hi].entryHI, page_table[hi].entryLO|dirtybit);
            splx(spl);
            lock_release(lock);
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

        lock_release(lock);

        //kprintf("Ran out of TLB entries - cannot handle page fault\n");

        return EFAULT;
    }

    if (pt_insert(as, faultaddress, newframe|TLBLO_VALID)) {
        free_kpages_frame(newframe>>12);
            lock_release(lock);
            return EFAULT;
    }

    spl = splhigh();
    tlb_random(faultaddress, newframe|dirtybit);
    splx(spl);
    lock_release(lock);
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

