#ifndef JOS_KERN_FILTER_H
#define JOS_KERN_FILTER_H


//*****************************************************************************************
// 					defines for filtering
//*****************************************************************************************

#define		ICMP	1
#define		IGMP	2
#define		TCP	6
#define		UDP	17
#define		ENCAP	41
#define		OSPF	89
#define 	SCTP	132

//*****************************************************************************************
// 					structs for filtering
//*****************************************************************************************

enum {
	VALUE_ANY,
	DIRECTION_IN,
	DIRECTION_OUT,
};

enum {
	ACTION_DENAY,
	ACTION_APPROVE,
	ACTION_DEN_WARN
};
enum {
	RANGE_ALL,
	RANGE_ONE, 	// check only or first byte
	RANGE_TWO, 	// check for two bytes
	RANGE_THREE, 	// check for three bytes
	RANGE_FOUR 	// check for specific ip
};

struct rule{
	char *name;
	int direction;
	uint32_t src_ip;
	uint32_t dst_ip;
	int range;
	int next_protocol;
	int action;
};

//*****************************************************************************************
// 					funcs decl
//*****************************************************************************************

int check_rule(int rule_num, uint32_t src_ip, uint32_t dst_ip, int protocol);

int check_packet(uint32_t src_ip, uint32_t dst_ip, int protocol);

#endif	// JOS_KERN_FILTER_H



