#ifndef FT_PYL_H
#define FT_PYL_H

#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <errno.h>            // errno, perror()
#include <ifaddrs.h>
#include <netinet/ether.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#define VID_ADDR_LEN           20
#define ETH_ADDR_LEN           5
#define MAX_BUFFER_SIZE        1024
#define MAX_INTERFACES         8

#define MTP_TYPE_JOIN_MSG       1  // Named as NULL MSG in OPNET.
#define MTP_TYPE_PERODIC_MSG   	2
#define MTP_TYPE_VID_ADVT	      3

#define MAX_MAIN_VID_TBL_PATHS  3

#define VID_ADD			            1
#define VID_DEL			            2

#define PVID_PORT 		          1 
#define NON_PVID_PORT		        2

#define PATH_COST		            0

#define PERIODIC_HELLO_TIME	    2.0

/* Type of Ports */
#define MTP_PORT                1
#define HOST_PORT               2

/* Container for VID Table */
struct vid_addr_tuple {
	char eth_name[ETH_ADDR_LEN]; 	// Port of Acquisition
	char vid_addr[VID_ADDR_LEN];	// VID Address.
	time_t last_updated;      	// last updated time
	int port_status;		// Port Type i.e. PVID, MTP
	int isNew;
	struct vid_addr_tuple *next;
	uint8_t path_cost;		// VID path cost.
	struct ether_addr mac;
	int membership;			// Membership PRIMARY, SECONDARY, TERTIARY
};

struct interface_tracker_t {
  char eth_name[ETH_ADDR_LEN];
  int isUP;               // true, if UP. false if DOWN.
  struct interface_tracker_t *next;
};

/*enum port_type_t {
	MT_PORT,
	HOST_PORT
};*/

/* Container for Child PVID Table */
struct child_pvid_tuple {
	char child_port[ETH_ADDR_LEN];  // Port name through which child is connected through
  char vid_addr[VID_ADDR_LEN];    // The VID of child, we might not need it, temporarily storing.
  struct ether_addr mac;
	struct child_pvid_tuple *next;
  time_t last_updated;        // last updated time
};

/* Local host broadcast ports Table */
struct local_bcast_tuple {
  char eth_name[ETH_ADDR_LEN];          // Host port Name
  struct local_bcast_tuple *next;
};

/* Function Prototypes for payloads */
int  build_JOIN_MSG_PAYLOAD(uint8_t *);
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *);
int  build_VID_ADVT_PAYLOAD(uint8_t *, char *);
int  build_VID_CHANGE_PAYLOAD(uint8_t *, char *, char **, int);     // params - payload, interfacename, deleted VIDS, number of deleted VIDs
bool isMain_VID_Table_Empty();
int isChild(char *);

/* Function Prototypes for MAIN VID Table Linked List */
bool add_entry_LL(struct vid_addr_tuple *);
bool find_entry_LL(struct vid_addr_tuple *);
void print_entries_LL();
bool update_hello_time_LL(struct ether_addr *);
struct vid_addr_tuple* getInstance_vid_tbl_LL();
bool delete_entry_LL(char *);

/* Function Prototypes for BKP VID Table Linked List */
void print_entries_bkp_LL();

/* Function Prototypes for CPVID table information */
bool add_entry_cpvid_LL(struct child_pvid_tuple *);
bool find_entry_cpvid_LL(struct child_pvid_tuple *);
void print_entries_cpvid_LL();
struct child_pvid_tuple* getInstance_cpvid_LL();
bool delete_entry_cpvid_LL(char *);
bool delete_MACentry_cpvid_LL(struct ether_addr *);
bool update_hello_time_cpvid_LL(struct ether_addr *);
bool update_entry_cpvid_LL(struct child_pvid_tuple *);

/* Function Prototypes for Local host broadcast information */
bool add_entry_lbcast_LL(struct local_bcast_tuple *);
bool find_entry_lbcast_LL(struct local_bcast_tuple *);
void print_entries_lbcast_LL();
bool delete_entry_lbcast_LL(char *port);
struct local_bcast_tuple* getInstance_lbcast_LL();
//void update_hello_time_cpvid_LL(struct ether_addr *);

/* check Failures */
int checkForFailures(char **);
bool checkForFailuresCPVID();
//bool isInterfaceActive(char *);
#endif /* FT_PYL_H */
