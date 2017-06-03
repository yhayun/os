// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800


// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if( !((uvpt[PGNUM(addr)] & (PTE_P|PTE_COW)) && (uvpd[PDX(addr)] & PTE_P) && (err & FEC_WR)) ){
		panic("user pagefault - PTE doesn't have appropriate COW flags - PTE: %d",uvpt[PGNUM(addr)]);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	int res;
	addr = ROUNDDOWN(addr, PGSIZE);	
	res = sys_page_alloc(0, (void *)PFTEMP, PTE_U | PTE_P| PTE_W);
	if(res)
		panic("COW failed - cannt alloc new page.");
	memcpy((void *)PFTEMP, addr, PGSIZE);
	sys_page_map(0, (void *)PFTEMP, 0, addr, PTE_U|PTE_P|PTE_W);
	if(res)
		panic("COW failed - cannt map new page.");
	sys_page_unmap(0, (void *)PFTEMP);
	
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r, perms;
	// LAB 4: Your code here.
	int pte = uvpt[pn];
	perms = PTE_P|PTE_COW;
	perms |= (pte & PTE_U);
	
	// LAB 5 Addition: (sharing):
	if((pte & PTE_SHARE)){
		//will put on default mappings with PTE_SYSCALL masking.
		//map child:
		r = sys_page_map(thisenv->env_id, (void*)(pn*PGSIZE) , envid, (void*)(pn*PGSIZE) , (pte & PTE_SYSCALL)) ;
		if(r)
			panic("duppage failed - map sharing gone wrong.");
		return 0;
	}

	//before enabled sharing code:
	if( ((pte & PTE_COW) || (pte & PTE_W))){
		//map child:
		r = sys_page_map(thisenv->env_id, (void*)(pn*PGSIZE) , envid, (void*)(pn*PGSIZE) , perms) ;
		if(r)
			panic("duppage failed - cannt map new page.");
		//map parent
		r = sys_page_map(thisenv->env_id, (void*)(pn*PGSIZE) , thisenv->env_id, (void*)(pn*PGSIZE) , perms) ;
		if(r)
			panic("duppage failed - cannt map new page.");
	}else{
		r = sys_page_map(thisenv->env_id, (void*)(pn*PGSIZE) , envid, (void*)(pn*PGSIZE) , PTE_P|(pte & PTE_U)) ;
		if(r)
			panic("duppage failed - cannt map new page.");
	}	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int pn,res,pdx;
	pte_t* pte;
	set_pgfault_handler(pgfault);
	uint32_t envid = sys_exofork();
	if (envid < 0 )
		panic("fork - exofork failed.");
	if(envid == 0){ 
	//child:
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	//parent:
	for (pn = 0; pn < PGNUM(UTOP - PGSIZE); pn++) {
		pdx = ROUNDDOWN(pn, NPDENTRIES) / NPDENTRIES;
		if ((uvpd[pdx] & PTE_P)  && ((uvpt[pn] & PTE_P) )) {
				duppage(envid, pn);
		}
	}
	res = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if(res < 0)
		panic("fork failed to alloc page for exception stack.");
	res = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if(res < 0)
		panic("fork failed to set childs handler.");

	res = sys_env_set_status(envid, ENV_RUNNABLE);
	if (res <0)
		panic("fork failed to set child status.");
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}


