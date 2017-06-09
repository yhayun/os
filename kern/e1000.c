#include <kern/e1000.h>
#include <kern/sched.h>
// LAB 6: Your driver code here
volatile uint32_t *e1000;


//**************************************************************************************
//		STRUCTURES:
//**************************************************************************************
//Transfer srtcutres:
struct tx_desc tx_desc_list[NUM_TRANS_DESC] __attribute__ ((aligned (16)));
struct tx_buf tx_buffer[NUM_TRANS_DESC] __attribute__ ((aligned (BUFFER_SIZE)));

//Receive structures:
struct rx_desc rx_desc_list[NUM_REC_DESC] __attribute__ ((aligned (16)));
struct rx_buf rx_buffer[NUM_REC_DESC] __attribute__ ((aligned (BUFFER_SIZE)));
struct rx_buf* zero_buf;

uint16_t MAC_eeprom[3];
//**************************************************************************************


int
pci_vendor_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	uint32_t bar0 = pcif->reg_base[0];
	e1000 = mmio_map_region(bar0, pcif->reg_size[0]);
	assert(e1000[E1000_STATUS] == 0x80080783);

	// EEPROM Challenge:
	read_epprom();
	set_mac();
	//setting up the hard wired mac address
 	//e1000[E1000_RAL] = 0x12005452;
 	//e1000[E1000_RAH] = 0x00005634 | E1000_RAH_AV;


	init_transmit();
	init_receive();
	
	// Enable Interrupts:
	//e1000[E1000_IMS] |= E1000_ICR_TXQE;//transmit queue emptry.
	e1000[E1000_IMS] |= E1000_ICR_RXT0;//Receive time - for receive queue full

	return 0;
}





//********************************************************************
// Init registers for trnasmit (ex.5): (chapter 14.5 in e1000 manual)
//********************************************************************
static void init_transmit(){

	//Program the Transmit Descriptor Base Address (TDBAL/TDBAH) register(s) :
	e1000[E1000_TDBAL] = PADDR(&tx_desc_list);	
	e1000[E1000_TDBAH] = 0x0; //We're only using 32bit so register isn't requried.

	//Allocate a region of memory for the transmit descriptor list:
	memset(tx_desc_list, 0, sizeof(struct tx_desc) * NUM_TRANS_DESC);
	int i;

	//Set the Transmit Descriptor Length
	e1000[E1000_TDLEN] = TRANS_DESC_SIZE; 

	//init The Transmit Descriptor Head and Tail:
	e1000[E1000_TDT] = 0x0;
	e1000[E1000_TDH] = 0x0;

	//Initialize the Transmit Control Register:
	e1000[E1000_TCTL] |= E1000_TCTL_EN;
	e1000[E1000_TCTL] |= E1000_TCTL_COLD;
	e1000[E1000_TCTL] |= E1000_TCTL_CT;
	e1000[E1000_TCTL] |= E1000_TCTL_PSP;

	//Program the Transmit TIPG:
	// Inside reg settings in bit range: {Table 13-77. TIPG Register Bit Description}
	// IPGT (9:0) = 10 , IPGR1 (19:10) = 8 , IPGR2 (29:20) = 12 ,reserved (31:30) = 0b.
	e1000[E1000_TIPG] = 0x0; // clear (instead of setting reserved to 0).	
	e1000[E1000_TIPG] |= GET_MASKED_VALUE(0xa, E1000_IPGT);
	e1000[E1000_TIPG] |= GET_MASKED_VALUE(0x8, E1000_IPGR1);
	e1000[E1000_TIPG] |= GET_MASKED_VALUE(0xc, E1000_IPGR2);
	for (i=0; i < NUM_TRANS_DESC; i++){
		tx_desc_list[i].addr = PADDR(&tx_buffer[i]);

		//We used modified flags to fit our struct derived from the orignial flags:
		tx_desc_list[i].cmd &= ~ E1000_TXD_DEXT; //set legacy mode.
		tx_desc_list[i].cmd |= E1000_TXD_RS; //enable report status
		tx_desc_list[i].status |= E1000_TXD_DD;// set desc done -> 'ready'.		
	}
}



//*********************************************************************
// Init registers for recieving (ex.10): (chapter 3.2 in e1000 manual)
//*********************************************************************
static void init_receive(){

	//Allocate a region of memory for the receive descriptor list:
	memset(rx_desc_list, 0, sizeof(struct rx_desc) * NUM_REC_DESC);
	int i;
	for (i=0; i < NUM_REC_DESC; i++){
		rx_desc_list[i].addr = PADDR(&rx_buffer[i]);
	}

	//Set the Receive Descriptor Length
	e1000[E1000_RDLEN] = TRANS_REC_SIZE; 

	//Program the Receive Descriptor Base Address register(s) :
	e1000[E1000_RDBAL] = PADDR(&rx_desc_list);	
	e1000[E1000_RDBAH] = 0x0; //We're only using 32bit so register isn't requried.

	//init The Receive Descriptor Head and Tail:
	e1000[E1000_RDT] = NUM_REC_DESC - 1;
	e1000[E1000_RDH] = 0x0;

	//RCTL flags intializtion
	e1000[E1000_RCTL] = 0;
	e1000[E1000_RCTL] &= ~E1000_RCTL_LPE;
	e1000[E1000_RCTL] |= E1000_RCTL_LBM_NO;
	e1000[E1000_RCTL] |= E1000_RCTL_SZ_2048;
	e1000[E1000_RCTL] |= E1000_RCTL_SECRC;
	e1000[E1000_RCTL] |= E1000_RCTL_EN;

}

//**************** ZERO COPY TRANSMIT ********************//
//ZERO COPY - will use walkpgdir to get the user envs pte for the
//buffer. then it will use it to get to the physcall address of 
// the pacakge and use it as the buffer for transmit. without copying.
// we need to make sure we don't resume the user env until copy is
// done otherwise it might change the buffer while transmit is working!
//**********************************************************//
// Returns 0 on success.
// Returns -1 when sender needs to try again.
int transmit_packet (void* package, int size){
	if( size > BUFFER_SIZE )
		panic("Transmit packet received size bigger than BUFFER");
	if(!package)
		panic("Transmit Packet got a null package");
	int cur_desc = e1000[E1000_TDT];

	if( !(tx_desc_list[cur_desc].status & E1000_TXD_STAT_DD) ){	
		return -1;//the queue is full - let sender know to try again.
	}
	
	//normal copying code:
	//--------------------
	//uint32_t addr = tx_desc_list[cur_desc].addr;
	//memcpy((void*)(KADDR(addr)) ,package, size);

	//zero-copy:
	//-------------
	//get physical addr of package:
	physaddr_t addr = (PTE_ADDR(*pgdir_walk(curenv->env_pgdir,package,0)) + PGOFF(package));
	//point descriptor to the pacakge phys addr 
	tx_desc_list[cur_desc].addr = addr;	


	tx_desc_list[cur_desc].status &=  ~E1000_TXD_STAT_DD;//mark used.
	tx_desc_list[cur_desc].length = size;
	tx_desc_list[cur_desc].cmd |= E1000_TXD_EOP;

	e1000[E1000_TDT] = ( cur_desc + 1) % NUM_TRANS_DESC; // advance iterator TDT.
	
	//make u-env sleep(yet runnnable) until the package is sent to it won't touch package mid transmit:
	while ( !(tx_desc_list[cur_desc].status & E1000_TXD_STAT_DD) ){
		sched_yield();
	}


	return 0;
}


// Returns the 'received packe size' on success.
// container must be of at least BUFFER_SIZE.
// return 	-E_NO_PACKAGE	when packet not found.
int receive_packet (void* container, int* size){
	if (!container)
		panic("Receive Packet got a null container");
	int idx = ( e1000[E1000_RDT] + 1) % NUM_REC_DESC;
	if( !(rx_desc_list[idx].status & E1000_RXD_STAT_DD) ){	
		//send to sleep on packet - let the system call handle putting us to sleep:
		return -E_NO_PACKAGE;
	}
	if ( !(rx_desc_list[idx].status & E1000_RXD_STAT_EOP) )
		panic("Receive: Expected only a single pacakge message, not a stream!");

	memcpy(container ,&rx_buffer[idx] ,rx_desc_list[idx].length);
	rx_desc_list[idx].status = 0;
	e1000[E1000_RDT] = idx; // advance iterator TDT.
	*size = rx_desc_list[idx].length;
	return 0;
}

//**************** ZERO COPY RECEIVE ********************//
// Unlike the zero copy trans this is a seperated function which we can choose to enable or not
// depends on the ZERO_COPY_ENABLE flag in the input.c file which starts the receive flow.

//We map the package pointer to point to pre allocated (shared) page instead of copying, the u-env will
// use this page via the package pointer later on. 
// we have to take care of mapping the buffer into the curenv (receiving env) addrss space and make sure
// the buffers are aligned for NIC to use them properly.
//**********************************************************//
int zero_receive(char** package){
	int size;

	//flag to override the normal init if we get here the first time - (using zero copy).
	static int initialized;
	if (!initialized)
	{
		init_zero_copy_receive();
		//next to we enter the function, we won't init again.
		initialized = 1;
	}


	int idx = ( e1000[E1000_RDT] + 1) % NUM_REC_DESC;
	if( !(rx_desc_list[idx].status & E1000_RXD_STAT_DD) ){	
		//send to sleep on packet - let the system call handle putting us to sleep:
		return -E_NO_PACKAGE;
	}
	if ( !(rx_desc_list[idx].status & E1000_RXD_STAT_EOP) )
		panic("Receive: Expected only a single pacakge message, not a stream!");

	size = rx_desc_list[idx].length;
	*package = (char*)(ZEROCOPY_BASE + idx * PGSIZE);
	rx_desc_list[idx].status = 0;
	e1000[E1000_RDT] = idx; // advance iterator TDT.
cprintf("----first idx: %d , addr of pack: %x\n",idx,package);
	return size;
}


void init_zero_copy_receive(){
	int num_pages = NUM_REC_DESC;
	char *buf_base = (char*) zero_buf;
	int i;
	struct PageInfo* page;
	void* va;
	for (i = 0; i < num_pages; i++){
		page =  page_alloc(PTE_P|PTE_U|PTE_W);
		//check if alloc fails.
		rx_desc_list[i].addr = page2pa(page);
		va = (void*)(ZEROCOPY_BASE + i * PGSIZE);
		if (page_insert(curenv->env_pgdir, page, va , PTE_P|PTE_U|PTE_W) < 0)
			panic("[e1000] init zerocpy");
	}
}





//Handle receive interrupt by waking up sleeping receivers:
void e1000_rec_handler(){
	int i = 0;
	struct Env* target = NULL;

	//Look for a receiving env in all of the envs:
	for (i =0; i < NENV; i++){
		if ( envs[i].env_e1000_rec == false)
			continue;
		target = &envs[i];
		break;
	}	

	//found one:
	if (target){
		assert(target->env_status == ENV_NOT_RUNNABLE);
		target->env_e1000_rec = false;
		target->env_status = ENV_RUNNABLE;
	}

	//clear interrupt:
	e1000[E1000_ICR] |= E1000_ICR_RXT0;
	lapic_eoi();
	irq_eoi();

}


void e1000_trans_handler(){
	int i = 0;
	struct Env* target = NULL;
	//Look for a receiving env in all of the envs:
	for (i =0; i < NENV; i++){
		if ( envs[i].env_e1000_trans == false)
			continue;
		target = &envs[i];
		break;
	}	

	//found one:
	if (target){
		assert(target->env_status == ENV_NOT_RUNNABLE);
		target->env_e1000_trans = false;
		target->env_status = ENV_RUNNABLE;
	}

	//clear interrupt:
	e1000[E1000_ICR] |= E1000_ICR_TXQE;
	lapic_eoi();
	irq_eoi();

}





/** Challenge EEPROM **/
//************************************************************************************************
// 		MANUAL GUIDE TO USE EEPROM:
//************************************************************************************************
/*Software can use the EEPROM Read register (EERD) to cause the Ethernet controller to read a
word from the EEPROM that the software can then use. To do this, software writes the address to
read the Read Address (EERD.ADDR) field and then simultaneously writes a 1b to the Start Read
bit (EERD.START). The Ethernet controller then reads the word from the EEPROM, sets the Read
Done bit (EERD.DONE), and puts the data in the Read Data field (EERD.DATA). Software can
poll the EEPROM Read register until it sees the EERD.DONE bit set, then use the data from the
EERD.DATA field. Any words read this way are not written to hardware’s internal registers.


Ethernet Address (Words 00h-02h):
----------------------------------
 The Ethernet Individual Address (IA) is a six-byte field that must be unique for each Ethernet port
(and unique for each copy of the EEPROM image). The first three bytes are vendor specific. The
value from this field is loaded into the Receive Address Register 0 (RAL0/RAH0). For a MAC
address of 12-34-56-78-90-AB, words 2:0 load as follows (note that these words are byteswapped):
Word 0 = 3412
Word 1 = 7856
Word 2 - AB90    */

void read_epprom(){
	int i;
	//reading MAC words from eeprom to read_epprom[]
	for(i =0; i < 3; i++){
		//tell eeprom to read from address i:
		e1000[E1000_EERD] |=  ( E1000_EEPROM_RW_REG_START|(i << E1000_EEPROM_RW_ADDR_SHIFT) );
		
		while ( !(e1000[E1000_EERD] & E1000_EEPROM_RW_REG_DONE) ){
			//wait till eeprom finishes reading: 
		}

		//write the word to our MAC_eeprom array[] from the data part of register:
		MAC_eeprom[i] = (e1000[E1000_EERD] >> E1000_EEPROM_RW_REG_DATA);
	
		//clear the EERD so we can read the next word:
		e1000[E1000_EERD] = 0;
	}

	//the MAC_eeprom[] should contain the MAC address now.
	//cprintf("mac addr: [%x] , [%x] , [%x]\n", MAC_eeprom[0], MAC_eeprom[1], MAC_eeprom[2]);
	return;
}

void set_mac(){
	// Set MAC address in (RAH/RAL).Set valid bit in E1000_RAH.
	e1000[E1000_RAL] = MAC_eeprom[0] |( MAC_eeprom[1] << E1000_EEPROM_RW_REG_DATA);
	e1000[E1000_RAH] = MAC_eeprom[2] | E1000_RAH_AV;
}

//used for system call:
void send_mac(void* mac_addr){
	if (mac_addr){
		*((uint32_t*) mac_addr)    = (uint32_t) e1000[E1000_RAL];
		*((uint16_t*)(mac_addr+4)) = (uint16_t) e1000[E1000_RAH];
	}
}




