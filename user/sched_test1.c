// Ping-pong a counter between two processes.
// Only need to start one of these -- splits into two with fork.

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	envid_t who;
	int i, p;
	int my_prio;
	for(i = 100; i >= 0; i--){
		if ((who = fork()) == 0) {
			p = (i % MAX_PRIO)? (i % MAX_PRIO): 1;
			sys_set_priority(p);
			my_prio = i % MAX_PRIO;
			//cprintf("my env id is %d, and i changes my priority to %d\n", sys_getenvid(), i % MAX_PRIO);
			//cprintf("Going to yield the CPU now. BYE BYE\n");
			goto stop;
		}
	}
	if (who)
		return;
stop:
	sys_yield();
	cprintf("Good Morning. and my prio is %d\n", sys_get_priority());
	
}
