#include "ns.h"
#define NUM_OF_RETRIES 10

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int sender;
	int r, perm;
	union Nsipc *pkt= (union Nsipc *)UTEMP;
	sys_page_alloc(0, (void *)UTEMP, PTE_P | PTE_U| PTE_W);
	//infinte loop:
	for (;;){
		//sleeps waiting for package:
		ipc_recv(&sender, (void *)pkt, &perm);
		if (sender != ns_envid){	
			cprintf("Warning: Received packet from unkown source, packet dumped.");
			continue;
		}
		r = sys_send_packet( pkt->pkt.jp_data, pkt->pkt.jp_len);cprintf("\n\n\nAFTER SYSCALL\n\n\n");
		if(r == -E_INVAL){
			cprintf("Warning: Packet size too big or null buffer");
			continue;
		}
	}
	panic("Shouldn't leave for loop! OMG");
}

