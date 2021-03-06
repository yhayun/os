// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>
#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display stack information", mon_backtrace },
	{ "showmappings", "Display physcal addresses", mon_showmappings },
	{ "setperm", "Display physcal addresses", mon_setperm },
	{"freespace", "show amount of free space in the system", mon_freespace},
	{"reftopa", "display number of references to a given pa",mon_reftopa},
	{"addrlayout", "Display the right Layout to given addresses", mon_addrlayout},
	{ "dump",     "Display content of va/pa address",     mon_dump},
	{ "continue",     "Continue from breakpoint",     mon_continue},
	{ "si",     "single step from breakpoint",     mon_si}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp = read_ebp();
	struct Eipdebuginfo info;
	while( ebp != 0x0 ){
		cprintf("  ebp %x  eip %x  args %08x %08x %08x %08x %08x\n", ebp, *(int*)(ebp+4), *(int*)(ebp+8), *(int*)(ebp+12),*(int*)(ebp+16),*(int*)(ebp+20),*(int*)(ebp+20));
		
		debuginfo_eip( *(int*)(ebp+4) , &info);
		cprintf("   %s:%d: %.*s+%d \n" ,info.eip_file , info.eip_line ,info.eip_fn_namelen, info.eip_fn_name, (*(int*)(ebp+4) - info.eip_fn_addr) );
		ebp = *(int*)ebp;
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}


//lab2 added challenge code:


// we except only number in a good range and a good format ( no more the 8 digits (not include 0x at the begging)
int isLegalHex(const char * str, int len){
    int i;
    if (len <=2 || str[0] != '0' || (str[1] != 'x' && str[1] != 'X') || len > 10)
        return 0;
    for (i = 2; i < len; i++)
        if (!((str[i] >= '0' && str[i] <= '9') || ( (str[i] >='a' && str[i] <='f') || (str[i] >='A' && str[i] <='F') )))
            return 0;
    return 1;
}

//va is pte.
void printPerm(int va){
    cprintf("| ");
    if ( va & PTE_P)
        cprintf("PTE_P | ");
    if ( va & PTE_W)
        cprintf("PTE_W | ");
    if ( va & PTE_U)
        cprintf("PTE_U | ");
    if ( va & PTE_PWT)
        cprintf("PTE_PWT | ");
    if ( va & PTE_PCD)
        cprintf("PTE_PCD | ");
    if ( va & PTE_A)
        cprintf("PTE_A | ");
    if ( va & PTE_D)
        cprintf("PTE_D | ");
    if ( va & PTE_PS)
        cprintf("PTE_PS | ");
    if ( va & PTE_G)
        cprintf("PTE_G | ");
    cprintf("\n");
}

void printMappings(uint32_t va_start, uint32_t va_end){
	cprintf("Memory Mappings:\n");
	cprintf("Virtual		Physical		Permissions\n");
	while(va_start <= va_end){
		pte_t* pte = pgdir_walk(kern_pgdir, (void *)va_start, 0);
		cprintf("%x		%x		", ROUNDDOWN(va_start, PGSIZE) ,pte ? PTE_ADDR(*pte) : 0);
		pte ? printPerm(*(int*)pte) : cprintf("N/A\n"); ;
		va_start += PGSIZE;
	} 
}

 int mon_showmappings(int argc, char **argv, struct Trapframe *tf){
	if(argc <= 1 || argc > 3){
		cprintf("invalid number of arguments\n");
		return 1;
	}else if(argc == 2){//single page print:
		if( isLegalHex(argv[1],strlen(argv[1])) == false ){
			cprintf("invalid address format\n");
		}	
		char * va_ptr = (argv[1] + strlen(argv[1]) + 1);
		uint32_t va = (uint32_t) strtol(argv[1], &va_ptr, 16);
		cprintf("\n%ul\n", va);
		printMappings(va,va);
	}else{//3 args, range print:
		if(argc > 2){
			if( (isLegalHex(argv[1],strlen(argv[1])) == false) || (isLegalHex(argv[2],strlen(argv[2])) == false) ){
				cprintf("invalid address format range. needs start and end address\n");
				return 1;
			}
		}else{
			cprintf("invalid number of arguments\n");
			return 1;	
		}
		char * va_ptr_start = (argv[1] + strlen(argv[1]) + 1);
		char * va_ptr_end = (argv[2] + strlen(argv[2]) + 1);

		uint32_t va_start = (uint32_t) strtol(argv[1], &va_ptr_start, 16);
		uint32_t va_end = (uint32_t) strtol(argv[2], &va_ptr_end, 16);
		printMappings(va_start, va_end);	
	}
	return 0;
}


void editPremissions(int va, int va_end, int perm){
    pde_t *pde;	
    while(va <= va_end){
	pde = &kern_pgdir[PDX(va)];//for pd permissions change as well.
        pte_t* pte = pgdir_walk(kern_pgdir ,(void*)va ,0 );
        if (pte){
       		*pte = (PTE_ADDR(*pte) | perm);
		*pde = (PTE_ADDR(*pde) | perm);
	}
        va += PGSIZE;
    }
	cprintf("Done!\n");
}

// assumes its get a VALID hex number !
void printLayout(uint32_t hexNum){
	if (hexNum >= 0x0 && hexNum < 0x00200000)
		cprintf("Empty Memory\n");
	else if (hexNum >= 0x00200000 && hexNum < 0x00400000)
		cprintf("USTABDATA\n");
	else if (hexNum >= 0x00400000 && hexNum < 0x007ff000)
		cprintf("UTEMP\n");
	else if (hexNum >= 0x007ff000 && hexNum < 0x00800000)
		cprintf("PFTEMP\n");
	else if (hexNum >= 0x00800000 && hexNum < 0xeebfd000)
		cprintf("UTEXT\n");
	else if (hexNum >= 0xeebfd000 && hexNum < 0xeebfe000)
		cprintf("Normal user stack\n");
	else if (hexNum >= 0xeebfe000 && hexNum < 0xeebff000)
		cprintf("Empty memory\n");
	else if (hexNum >= 0xeebff000 && hexNum < 0xeec00000)
		cprintf("UXSTACKTOP\n");
	else if (hexNum >= 0xeec00000 && hexNum < 0xef000000)
		cprintf("UTOP, UENVS\n");
	else if (hexNum >= 0xef000000 && hexNum < 0xef400000)
		cprintf("UPAGES\n");
	else if (hexNum >= 0xef400000 && hexNum < 0xef800000)
		cprintf("UVPT\n");
	else if (hexNum >= 0xef800000 && hexNum < 0xefc00000)
		cprintf("ULIM, MMIOBASE\n");
	else if (hexNum >= 0xefc00000 && hexNum < 0xf0000000)
		cprintf("MMIOLIM, CPUS Stacks and Invalid Memoery (stack gurading)\n");
	else if (hexNum >= 0xf0000000 && hexNum < 0xffffffff)
		cprintf("KERNBASE, Kernel memory\n");
}


int makePerm(int argc, char **argv, int arrStart){
    int perms = 0;
    
    while (arrStart < argc){
        if (!strcmp(argv[arrStart],"PTE_P"))
            perms |= PTE_P;
        if (!strcmp(argv[arrStart],"PTE_W"))
            perms |= PTE_W;
        if (!strcmp(argv[arrStart],"PTE_U"))
            perms |= PTE_U;
        if (!strcmp(argv[arrStart],"PTE_PWT"))
            perms |= PTE_PWT;
        if (!strcmp(argv[arrStart],"PTE_PCD"))
            perms |= PTE_PCD;
        if (!strcmp(argv[arrStart],"PTE_A"))
            perms |= PTE_A;
        if (!strcmp(argv[arrStart],"PTE_D"))
            perms |= PTE_D;
        if (!strcmp(argv[arrStart],"PTE_PS"))
            perms |= PTE_PS;
        if (!strcmp(argv[arrStart],"PTE_G"))
            perms |= PTE_G;

	arrStart++;
    }
    return perms;
}


//if we get -r flag we accept range of addresses instead of a single one. 
int mon_setperm(int argc, char **argv, struct Trapframe *tf){
	if(argc <= 2){
		cprintf("invalid number of arguments\n");
		return 1;
	}else if( strcmp(argv[1],"-r") != 0){// no -r flag: 'setperm add perms...'
		if( isLegalHex(argv[1],strlen(argv[1])) == false ){
			cprintf("invalid address format\n");
			return 1;
		}		
		char * va_ptr = (argv[1] + strlen(argv[1]) + 1);
		int perm = makePerm(argc, argv, 2);
		uint32_t va = (uint32_t) strtol(argv[1], &va_ptr, 16);
		editPremissions(va,va,perm);
	}else{// received -r flag: 'setperm -r add1, add2, perms...'
		if(argc > 3){
			if( (isLegalHex(argv[3],strlen(argv[3])) == false) || (isLegalHex(argv[2],strlen(argv[2])) == false) ){
				cprintf("invalid address format range. needs start and end address\n");
				return 1;
			}
		}else{
			cprintf("invalid number of arguments\n");
			return 1;	
		}
		char * va_ptr_start = (argv[2] + strlen(argv[2]) + 1);
		char * va_ptr_end = (argv[3] + strlen(argv[3]) + 1);
		int perm = makePerm(argc, argv, 4);
		uint32_t va_start = (uint32_t) strtol(argv[2], &va_ptr_start, 16);
		uint32_t va_end = (uint32_t) strtol(argv[3], &va_ptr_end, 16);
		editPremissions(va_start, va_end, perm);	
	}
	return 0;
}


int mon_freespace(int argc, char **argv, struct Trapframe *tf){
	cprintf("Amount of free space is %dKB/%dKB\n",number_of_freePages() * PGSIZE/1024, npages * PGSIZE/1024);
	return 0;
}

int mon_reftopa(int argc, char **argv, struct Trapframe *tf){
	int i;
	char *pa_ptr;
	uint32_t pa;
	struct PageInfo* pp;	
	if (argc <2 ){
		cprintf("not enough arguments !");
		return 1;	
	}
	for (i = 1; i < argc; i++){
		pa_ptr = (argv[i] + strlen(argv[i]) + 1);
		pa = (uint32_t) strtol(argv[i], &pa_ptr, 16);
		pp = pa2page(pa);
		cprintf("the physical address %p: has %d references\n", pa, pp->pp_ref);
	}
	return 0;
}

int mon_addrlayout(int argc, char **argv, struct Trapframe *tf){
	int i;	
	if (argc < 2)
		cprintf("not enough arguments");
	for ( i = 1; i < argc; i++){
		if (!isLegalHex(argv[i], strlen(argv[i]))){
			cprintf("one (or more) of the args are not legel hex !");
			return 1;
		}
	}
	for ( i = 1; i < argc; i++){
	    char * va_ptr = (argv[i] + strlen(argv[i]) + 1);
 	    uint32_t va = (uint32_t) strtol(argv[i], &va_ptr, 16);
	    cprintf("address: %s ", argv[i]);
	    printLayout(va);
	}
	return 0 ;
}

int mon_dump(int argc, char **argv, struct Trapframe *tf){
    int jumps = 1;
    int addrDiff = 0;
    int count = 0;
    pte_t* pte = NULL;
    if ( argc != 4 ){
        cprintf("invalid number of arguments, should be- <dump> <-p/-v> addr1 addr2\n");
	return 1;
    }
    if (!isLegalHex(argv[2],strlen(argv[2])) || !isLegalHex(argv[3],strlen(argv[3]))){
        cprintf("One or more of the addressess is illegal ! address should be - 0xnnnnnnnn (not more then 8 digits !\n");
        return 1;
    }
    if ((strcmp(argv[1],"-v") && strcmp(argv[1],"-p")))
        cprintf("Illegal argument :(\n");
    char * va_ptr_start = (argv[2] + strlen(argv[2]) + 1);
    char * va_ptr_end = (argv[3] + strlen(argv[3]) + 1);
    uint32_t va_start = (uint32_t) strtol(argv[2], &va_ptr_start, 16);
    uint32_t va_end = (uint32_t) strtol(argv[3], &va_ptr_end, 16);
    if (!(strcmp(argv[1],"-p")) && (va_start >= KERNBASE || va_end >= KERNBASE)){//lets not make the kernel panic !
          cprintf("the phys address are to big !\n");
	  return 1;
    }
    if (!(strcmp(argv[1],"-p"))){
	va_start = (uint32_t)KADDR(va_start);
	va_end = (uint32_t)KADDR(va_end);	
    }
    while (va_start <= va_end){
        pte = pgdir_walk(kern_pgdir ,(void *)va_start ,0 );
	addrDiff = va_end - va_start > PGSIZE ? //here is a checking for how much we should go with allinments !
			(!(ROUNDUP(va_start, PGSIZE) - va_start) ? PGSIZE : ROUNDUP(va_start, PGSIZE) - va_start) : va_end - va_start;
       if (pte){// if pte - then a page is allocated !
            while(addrDiff > count && (va_start + count <= va_end)){
		if (!strcmp(argv[1],"-p"))
                	cprintf("address: %08x, data: %x\n", va_start + count - KERNBASE, *(pte_t *)(va_start + count));
		else
			cprintf("address: %08x, data: %x\n", va_start + count,*(pte_t *)(va_start + count));
		count += 4;
            }
	    count = 0;
        }
        va_start = va_start + PGSIZE;
        
    }
    return 0;
}


//lab3 - challenge code:
int mon_continue(int argc, char **argv, struct Trapframe *tf){
	// Return to the current environment, which should be running.
	if(!(curenv && curenv->env_status == ENV_RUNNING)){
		panic("No breakpoint set! - Cannot contniue from here.");
	}
	env_run(curenv);

	//shouldn't get here.
	return 0;
}

int mon_si(int argc, char **argv, struct Trapframe *tf){
	// single step flag = FL_TF
	if(!(curenv && curenv->env_status == ENV_RUNNING)){
		panic("No breakpoint set! - Cannot contniue from here.");
	}
	curenv->env_tf.tf_eflags |= FL_TF ;
	env_run(curenv);
	return 0;
}








