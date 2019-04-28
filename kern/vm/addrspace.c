/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

static int
append_region(struct addrspace *as, char permissions, vaddr_t start, size_t size);

// destroy pt
static void pt_destroy(struct addrspace *as);

// dup pt
static int pt_dup(struct addrspace *new, struct addrspace *old);

static int create_pt_entry(struct addrspace *as, int index);

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	bzero(as, sizeof(*as));
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;
    int err;
	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
    
    struct region *cur = old->regions;
    while(cur){
        err = append_region(newas, cur->cur_perms, cur->start, cur->size); 
        if(err)
            return err;
        cur = cur->next;
    }
    
    newas->isLoading = old->isLoading;
	err = pt_dup(newas, old);
    if(err)
        return err;
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	if(as == NULL)
		return;
	
	struct region *cur = as->regions;
	struct region *temp = NULL;
	while(cur){
		temp = cur->next;
		kfree(cur);
		cur = temp;
	}
	pt_destroy(as);
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if(as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for(i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */

	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if(as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for(i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	// npages = sz / PAGE_SIZE;
	char r, w, e;
	r = readable ? READ : 0;
	w = writeable ? WRITE : 0;
	e = executable ? EXE : 0;
	char p = r | w | e;
	int err = append_region(as, p, vaddr, memsize);

	if(err){
		return err;
	}

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	if(as == NULL)
		return ENOMEM;

	/*
	struct region *cur = as->regions;
    while(cur) {
		cur->cur_perms |= WRITE;
		cur = cur->next;
    }
	*/
	as->isLoading = true;
	as_activate();
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	if(as == NULL)
		return ENOMEM;

	/*
	struct region *cur = as->regions;
    while(cur) {
		cur->cur_perms = cur->ori_perms;
		cur = cur->next;
    }
	*/
	as->isLoading = false;
	as_activate();
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	if(as == NULL)
		return ENOMEM;

	int err = as_define_region(as, USERSTACK - USERSTACK_SIZE, 
			USERSTACK_SIZE, READ, WRITE, 0);

	if(err)
		return err;
	
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

static int
append_region(struct addrspace *as, char permissions, vaddr_t start, size_t size){
	struct region *new = NULL;
	struct region *prev = NULL;
	struct region *cur = NULL;

	/* Align the region. First, the base... */
	size += start & ~(vaddr_t)PAGE_FRAME;
	start &= PAGE_FRAME;

	/* ...and now the length. */
	size = (size + PAGE_SIZE - 1) & PAGE_FRAME;

	new = kmalloc(sizeof(*new));
	if(!new){
		return ENOMEM;
	}

	new->cur_perms = permissions;
	new->size = size;
	new->start = start;
	new->next = NULL;

	cur = as->regions;
	prev = cur;
    if(cur == NULL) {
        as->regions = new;
		return 0;
	}

	while(cur && (cur->start + cur->size <= new->start)){
		prev = cur;
		cur = cur->next;	
	}
	/* 
	 * prev->new->cur
	 * we need to check whether new's start is less than or equal to prev + size
	 * new's end is less than or equal to prev + size
	*/
	if(cur != NULL && (new->start + new->size > cur->start))
		return EADDRINUSE;	
	prev->next = new;
	new->next = cur;
	return 0;
}

/*
* destroy pagetable
*/
static void pt_destroy(struct addrspace *as)
{
	struct entry *entry;
	
	for(int i = 0; i < TABLE_SIZE; i++){
		entry = as->page_table[i];

		if(entry){
			for(int j = 0; j < TABLE_SIZE; j++){
				if(entry[j].entrylo != 0x0){
					free_kpages(PADDR_TO_KVADDR(entry[j].entrylo));
				}
			}
			kfree(entry);
		}

		as->page_table[i] = NULL;
	}
}


struct entry * pt_search(struct addrspace *as, vaddr_t addr)
{
	uint32_t out = addr >> 22;
	uint32_t in = addr << 10;
	in = in >> 22;
	struct entry *pe = NULL;
	if(as->page_table[out]){
		pe = as->page_table[out];
		if(pe[in].entrylo != 0x0){
			return pe + in;
		}
		pe = NULL;
	}
	return pe;
}

/* 
* dup pagetable
*/
static int pt_dup(struct addrspace *new, struct addrspace *old)
{
    struct entry *oe = NULL, *ne = NULL;
    uint32_t newframe;
    for(int i = 0; i < TABLE_SIZE; i++){
        oe = old->page_table[i];
		if(oe && !new->page_table[i]){
			new->page_table[i] = kmalloc(sizeof(struct entry) * TABLE_SIZE);
			bzero(new->page_table[i], TABLE_SIZE);
		}
		ne = new->page_table[i];
        for(int j = 0; oe && j < TABLE_SIZE; j++){
            if(oe[j].entrylo != 0x0){
                // alloc new frame
                newframe = alloc_kpages(1);
                if(newframe == 0x0)
                    return ENOMEM;
                // copy page entry
                memmove((void*) newframe, (const void *)PADDR_TO_KVADDR(oe[j].entrylo & PAGE_FRAME), PAGE_SIZE); 
                ne[j].permissions = oe[j].permissions;
				ne[j].entrylo = KVADDR_TO_PADDR(newframe) & PAGE_FRAME;
            }
        }
		new->page_table[i] = ne;
    }
    return 0;
}

static int create_pt_entry(struct addrspace *as, int index){
	struct entry *new = kmalloc(sizeof(*new) * TABLE_SIZE);
	if(!new)
		return ENOMEM;
	bzero(new, TABLE_SIZE);
	as->page_table[index] = new;
	return 0;
}

struct entry *pt_insert(struct addrspace *as, uint32_t lo, vaddr_t addr, char perms)
{
	uint32_t first_index = addr >> 22; 
	uint32_t sec_index = (addr << 10) >> 22;
	int err = 0;
	if(!as->page_table[first_index]){
		err = create_pt_entry(as, first_index);
	}
	
	struct entry *pe = as->page_table[first_index];
	pe[sec_index].entrylo = lo;
	pe[sec_index].permissions = perms;
	
	if(err){
		return NULL;
	}
	return pe + sec_index;
}

char region_perm_search(struct addrspace *as, vaddr_t addr){
	struct region *cur = as->regions;
	while(cur){
		if(cur->start <= addr && (cur->start + cur->size) > addr){
			return cur->cur_perms;
		}
		cur = cur->next;
	}
	return -1;
}
