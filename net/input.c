
#include "ns.h"
#include <inc/syscall.h>
#include <inc/lib.h>

#define BUFFER_SIZE 2048


//****************************************************//
//	ZERO COPY CHALLENGE
//****************************************************//
#define ZERO_COPY_ENABLE 1
static void input_zerocpy(envid_t ns_envid);
static int zerocpy_rec(char** package);
static void input_normal(envid_t ns_envid);
//***************************************************//

extern union Nsipc nsipcbuf;


void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	int res, size = 0;
	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	if (ZERO_COPY_ENABLE){
		// ZERO COPY //
		input_zerocpy(ns_envid);
	}else{
		//normal receive//
		input_normal(ns_envid);
	}
}


static void input_normal(envid_t ns_envid){
	int res, size = 0;
	sys_page_alloc(0, (void *)UTEMP, PTE_P|PTE_U|PTE_W);

	for (;;){
		while ( (res = sys_receive_packet((void *)(((union Nsipc *)UTEMP)->pkt.jp_data) ,&size)) < 0 ){	
			//the 'retry' loop in case the env went to sleep waiting for buffers and now woke up again
			//the sys call sends the env to sleep if buffers are full and when the env is woken up it
			// returns here, where we try to receive again.
			//the interrupt handler is responsible for waking the sleeping envs (one at the time) so
			// they could enter this while loop again.
		}
		if ((res = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_W|PTE_U)) < 0)
			panic("input: unable to allocate new page, error %e\n", res);

		memcpy(nsipcbuf.pkt.jp_data, ((union Nsipc *)UTEMP)->pkt.jp_data , size);
		nsipcbuf.pkt.jp_len = size;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U);
	}
	panic("Shouldn't leave for loop! OMG");
}


struct xxx{
	int size;
	char* pkt;
};

/**** ZERO COPY *****/
static void input_zerocpy(envid_t ns_envid){
	int res, size = 0,i,t;
	struct jif_pkt* pkt;
	struct xxx x;
	zerocpy_rec(NULL);//used to init zero copy buffers.
	for (;;){ 
		size = zerocpy_rec((char**) &pkt);
		ipc_send(ns_envid, NSREQ_INPUT, pkt, PTE_P|PTE_U|PTE_W);
	}
	panic("Shouldn't leave for loop! OMG");
}


static int zerocpy_rec(char** package){
	int res;

	while ( (res = sys_zero_receive(package)) < 0 ){
		//sleeps waiting for interrupt.
	}

	//when succeeded:
	return res;

}











