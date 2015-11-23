#include "feature_payload.h"
#include "mtp_send.h"

/* file locals */
struct vid_addr_tuple *main_vid_tbl_head = NULL;
//struct vid_addr_tuple *bkp_vid_tbl_head = NULL; // we can maintain backup paths in Main VID Table only, just a thought
struct child_pvid_tuple *cpvid_tbl_head = NULL; 
struct local_bcast_tuple *local_bcast_head = NULL;

/*
 *   isChild() - This method checks if the input VID param is child of any VID in Main 
 * 		 table or backup vid table.
 *   @input
 *   param1 -   char *       vid
 *
 *   @return
 *   2  - duplicate
 *   1 	- if is a child of one of VID's in main VID Table.
 *   0 	- if VID is parent ID of one of the VID's in the main VID Table.
 *  -1  - if VID is not child of any of the VID's in the main VID Table.
 *
 */

int isChild(char *vid) {

	// For now just checking only the Main VID table.
	if (main_vid_tbl_head != NULL) {
		struct vid_addr_tuple *current = main_vid_tbl_head;

		while (current != NULL){
		  int lenInputVID = strlen(vid);
      int lenCurrentVID = strlen(current->vid_addr);

      // This check is mainly if we get a parent ID, we have eliminate this as the VID is a parent ID.
      if (lenCurrentVID > lenInputVID && strncmp(current->vid_addr, vid, lenInputVID) == 0) {
        lenCurrentVID = lenInputVID;
        return 0;
      }

      // if length is same and are similar then its a duplicate no need to add.
      if ((lenInputVID == lenCurrentVID) && (strncmp(vid, current->vid_addr, lenCurrentVID)==0)) {

        return 2;
      }

			if (strncmp(vid, current->vid_addr, lenCurrentVID)==0) {
				return 1;
			} 
			current = current->next;
		}
	} 
	return -1;
}


/*
 *   VID Advertisment - This message is sent when a JOIN message is received from non MT Switch
 *   @input
 *   param1 -   uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS>  <PATH COST> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT> 
int  build_VID_ADVT_PAYLOAD(uint8_t *data, char *interface) {
  int payloadLen = 3;
  int numAdvts = 0;
  int egressPort = 0;

  struct vid_addr_tuple *current = main_vid_tbl_head;

  // Port from where VID request came.
  int i;
  for(; interface[i]!='\0'; i++) {
    if(interface[i] >= 48 && interface[i] <= 57) {
      egressPort = (egressPort * 10) + (interface[i] - 48);
    }
  }

  while (current != NULL) {
    char vid_addr[VID_ADDR_LEN];

    // <PATH COST> - Taken as '1' for now
    data[payloadLen] = (uint8_t) (current->path_cost + 1);

    // next byte
    payloadLen = payloadLen + 1;

    memset(vid_addr, '\0', VID_ADDR_LEN);
    if (strncmp(interface, current->eth_name, strlen(interface)) == 0) {
      sprintf(vid_addr, "%s", current->vid_addr );
    } else {
      sprintf(vid_addr, "%s.%d", current->vid_addr, egressPort );
    }

    // <VID_ADDR_LEN>
    data[payloadLen] = strlen(vid_addr);

    // next byte
    payloadLen = payloadLen + 1;

    memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));

    payloadLen += strlen(vid_addr);
    current = current->next;
    numAdvts++;

    /* VID Advts should not be more than 3, because we need to advertise only the entries that are in Main VID Table
       from 3 we consider every path as backup path. */
    if (numAdvts >=3 ) {
        break;
    }
  }

  if (numAdvts > 0) {
    // <MSG_TYPE> - Hello Join Message, Type - 3.
    data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

    // <OPERATION>
    data[1] = VID_ADD;

    // <NUMBER_VIDS> - Number of VID's
    data[2] = (uint8_t) numAdvts;
  } else {
    payloadLen = 0;
  }

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_JOIN_MSG_PAYLOAD - Join message sent when a non MTP switch wants to be
 *			      be part of MT_Topology. This switch Main VID Table is
 * 			      empty intially.
 *   @input
 *   param1 - 	uint8_t       payload buffer
 *
 *   @output
 *   payloadLen	
 *
 */

// Message ordering <MSG_TYPE>
int  build_JOIN_MSG_PAYLOAD(uint8_t *data) {
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Join Message, Type - 1.
  data[payloadLen] = (uint8_t) MTP_TYPE_JOIN_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_PERIODIC_MSG_PAYLOAD - This message is sent every 2 seconds to inform connected
 *                                peers about its perscence.
 *                                
 *   @input
 *   param1 -   uint8_t           payload buffer
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE>
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *data) {
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Periodic Message, Type - 2.
  data[payloadLen] = (uint8_t) MTP_TYPE_PERODIC_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_VID_CHANGE_PAYLOAD - This message is sent when ever we miss periodic message for a time of 3 * hello_time.
 *                              Respective VID is deleted and adjacent peers who have VID derived from deleted VID are notified.
 *
 *   @input
 *   param1     -   uint8_t           payload buffer
 *   interface  -   char              interface name
 *   deletedVIDs-   char**            all recently deleted VIDs
 *   numberOfDel-   int               total number of deletions deletedVIDs reference pointing to.
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
int  build_VID_CHANGE_PAYLOAD(uint8_t *data, char *interface, char **deletedVIDs, int numberOfDeletions) {
  int payloadLen = 3;
  int numAdvts = 0;
  int egressPort = 0;

  // Port from where VID request came.
  int i = 0;
  for(; interface[i]!='\0'; i++) {
    if(interface[i] >= 48 && interface[i] <= 57) {
      egressPort = (egressPort * 10) + (interface[i] - 48);
    }
  }

  i = 0;
  for (; i < numberOfDeletions; i++) {
    char vid_addr[VID_ADDR_LEN];

    memset(vid_addr, '\0', VID_ADDR_LEN);
    //sprintf(vid_addr, "%s.%d", deletedVIDs[i], egressPort ); for now, lets see if don't append egress port
    sprintf(vid_addr, "%s", deletedVIDs[i]);
    
    // <VID_ADDR_LEN>
    data[payloadLen] = strlen(vid_addr);

    // next byte
    payloadLen = payloadLen + 1;

    memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));
    payloadLen += strlen(vid_addr);
    numAdvts++;
  }

  if (numAdvts > 0) {
    // <MSG_TYPE> - Hello Join Message, Type - 3.
    data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

    // <OPERATION>
    data[1] = VID_DEL;

    // <NUMBER_VIDS> - Number of VID's
    data[2] = (uint8_t) numAdvts;
  } else {
    payloadLen = 0;
  }

  return payloadLen;
}
/*
 *   isMain_VID_Table_Empty -     Check if Main VID Table is empty.
 *
 *   @input
 *   no params
 *
 *   @return
 *   true  	- if Main VID Table is empty.
 *   false 	- if Main VID Table is not null.
 */

// Message ordering <MSG_TYPE>
bool isMain_VID_Table_Empty() {
        
	if (main_vid_tbl_head) {
		return false;
	}
	
  return true;
}

/**
 * 		Add into the VID Table, new VID's are added based on the path cost.
 *     		VID Table,		Implemented Using linked list. 
 * 		Head Ptr,		*vid_table
 * 		@return 
 * 		true 			Successful Addition
 * 		false 			Failure to add/ Already exists.		 	
**/

bool add_entry_LL(struct vid_addr_tuple *node) {
	struct vid_addr_tuple *current = main_vid_tbl_head;
	// If the entry is not already present, we add it.
	if (!find_entry_LL(node)) {
		if (main_vid_tbl_head == NULL) {
				node->membership = 1;
				main_vid_tbl_head = node;
		} else {
			struct vid_addr_tuple *previous = NULL;
			
			int mship = 0;	
			// place in accordance with cost, lowest to highest.
			while(current!=NULL && (current->path_cost < node->path_cost)) {
				previous = current;
				mship = current->membership;
				current = current->next;
			}

			// if new node has lowest cost.
			if (previous == NULL) {
				node->next = main_vid_tbl_head;
				node->membership = 1;
				main_vid_tbl_head = node;
			} else {
				previous->next = node;
				node->next = current;
				node->membership = (mship + 1);
			}

			// Increment the membership IDs of other VID's
			while (current != NULL) {
				current->membership++;
				current = current->next;
			}
		}
		return true;
	}
	return false;
}

/**
 *		Check if the VID entry is already present in the table.
 *      VID Table,		Implemented Using linked list. 
 * 		Head Ptr,		*vid_table	
 *		
 *		@return
 *		true			Element Found.
 *		false			Element Not Found.	 	
**/

bool find_entry_LL(struct vid_addr_tuple *node) {
	if (main_vid_tbl_head != NULL) {
		struct vid_addr_tuple *current = main_vid_tbl_head;
		while (current != NULL) {
			if (strcmp(current->vid_addr, node->vid_addr) == 0) {
				// Update time stamp.
				current->last_updated = time(0);
				return true;
			}
			current = current->next;
		}
	}
	return false;	
}

/**
 *		Print VID Table.
 *      	VID Table,		Implemented Using linked list. 
 * 		Head Ptr,		*vid_table	
 *		
 *		@return
 *		void	
**/

void print_entries_LL() {
  struct vid_addr_tuple *current;
  int tracker = MAX_MAIN_VID_TBL_PATHS;

  printf("\n#######Main VID Table#########\n");
  printf("MT_VID\t\t\t\tEthname\t\t\tPath Cost\tMembership\tMAC\n");

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (tracker <= 0) {
      break;
    } else {
      printf("%s\t\t\t\t%s\t\t\t%d\t\t%d\t\t%s\n", current->vid_addr, current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac) );
      tracker--;
    }
  }
}

/**
 *              Update timestamp for a MAC address on both VID Table and backup table.
 *     		VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              bool
**/

bool update_hello_time_LL(struct ether_addr *mac) {
  struct vid_addr_tuple *current;
  bool hasUpdates = false;

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
      current->last_updated = time(0);
      hasUpdates = true;
    }	
  }
  return hasUpdates;
}

/**
 *              Check for link Failures.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              void
**/

int checkForFailures(char **deletedVIDs) {
  struct vid_addr_tuple *current = main_vid_tbl_head;
  struct vid_addr_tuple *previous = NULL;
  time_t currentTime = time(0);
  int numberOfFailures = 0;

  while (current != NULL) {
    if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
      struct vid_addr_tuple *temp = current;
      deletedVIDs[numberOfFailures] = (char*)calloc(strlen(temp->vid_addr), sizeof(char));
      if (previous == NULL) {
        main_vid_tbl_head	= current->next;
      } else {
        previous->next = current->next;
      }
      strncpy(deletedVIDs[numberOfFailures], temp->vid_addr, strlen(temp->vid_addr));
      current = current->next;
      numberOfFailures++;
      free(temp);
      continue;
    }
    previous = current;
    current = current->next;
  }

  // if failures are there
  if (numberOfFailures > 0) {
    int membership = 1;
    for (current = main_vid_tbl_head; current != NULL; current = current->next) {
      current->membership = membership;
      membership++;
    }
  }
  return numberOfFailures;
}

/**
 *              Delete Entries in the vid_table using vid as a reference.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              true - if deletion is successful.
 *              false - if deletion fails.
**/

bool delete_entry_LL(char *vid_to_delete) {
  struct vid_addr_tuple *current = main_vid_tbl_head;
  struct vid_addr_tuple *previous = NULL;
  bool hasDeletions = false;

  while (current != NULL) {
    if (strncmp(vid_to_delete, current->vid_addr, strlen(vid_to_delete)) == 0) {
      struct vid_addr_tuple *temp = current;

      if (previous == NULL) {
        main_vid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      hasDeletions = true;
      free(temp);
      continue;
    }
    previous = current;
    current = current->next;
  }

  // fix any wrong membership values.
  if (hasDeletions) {
    int membership = 1;
    for (current = main_vid_tbl_head; current != NULL; current = current->next) {
      current->membership = membership;
      membership++;
    }
  }
  return hasDeletions;
}


/**
 *              Get Instance of Main VID Table.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              struct main_vid_tbl_head* - reference of main vid table.
**/

struct vid_addr_tuple* getInstance_vid_tbl_LL() {
  return main_vid_tbl_head;
}

/**
 *    Print VID Table.
 *    Backup VID Paths,    Implemented Using linked list, instead of maintaining a seperate table, I am adding Main VIDS and Backup Paths
 *                         in the same table. 
 *    Head Ptr,   *vid_table  
 *    
 *    @return
 *    void  
**/

void print_entries_bkp_LL() {
  struct vid_addr_tuple *current;
  int tracker = MAX_MAIN_VID_TBL_PATHS;

  printf("\n#######Backup VID Table#########\n");
  printf("MT_VID\t\t\t\tEthname\t\t\tPath Cost\tMembership\tMAC\n");

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (tracker <= 0) {
      printf("%s\t\t\t\t%s\t\t\t%d\t\t%d\t\t%s\n", current->vid_addr, current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac) );
    } else {
      tracker--;
    }
  }
}

/**
 *    Add into the Child PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/

bool add_entry_cpvid_LL(struct child_pvid_tuple *node) {
  if (cpvid_tbl_head == NULL) {
    cpvid_tbl_head = node;
    return true;
  } else if (update_entry_cpvid_LL(node))  {      // if already, there and there is PVID change.
    
  } else {
    if (!find_entry_cpvid_LL(node)) {
      node->next = cpvid_tbl_head;
      cpvid_tbl_head = node;
      return true;
    }
  }
  return false;
}

/**
 *    Check if the VID entry is already present in the table.
 *    Child PVID Table,  Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *    @return
 *    true      Element Found.
 *    false     Element Not Found.
**/

bool find_entry_cpvid_LL(struct child_pvid_tuple *node) {
  struct child_pvid_tuple *current = cpvid_tbl_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->vid_addr, node->vid_addr) == 0) {
        
        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print Child PVID  Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @return
 *    void
**/

void print_entries_cpvid_LL() {
  struct child_pvid_tuple *current;

  printf("\n#######Child PVID Table#########\n");
  printf("Child PVID\t\tPORT\t\t\tMAC\n");

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    printf("%s\t\t\t%s\t\t\t%s\n", current->vid_addr, current->child_port, ether_ntoa(&current->mac) );
  }
}

/**
 *    Get instance of child of PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

struct child_pvid_tuple* getInstance_cpvid_LL() {

  return cpvid_tbl_head;
}

/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @input 
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

bool delete_entry_cpvid_LL(char *cpvid_to_be_deleted) {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  bool hasDeletions = false;  

  while (current != NULL) {
    if (strncmp(cpvid_to_be_deleted, current->vid_addr, strlen(cpvid_to_be_deleted)) == 0) {
      struct child_pvid_tuple *temp = current;

      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @input 
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

bool delete_MACentry_cpvid_LL(struct ether_addr *mac) {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  bool hasDeletions = false;

  while (current != NULL) {
    if (memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0) {
      struct child_pvid_tuple *temp = current;

      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *              Update timestamp for a MAC address on both CPVID Table and backup table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool update_hello_time_cpvid_LL(struct ether_addr *mac) {
  struct child_pvid_tuple *current;
  bool isUpdated = false;

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
      current->last_updated = time(0);
      isUpdated = true;
    }
  }
  return isUpdated;
}

/**
 *              Update timestamp for a MAC address on both CPVID Table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool update_entry_cpvid_LL(struct child_pvid_tuple *node) {
  struct child_pvid_tuple *current;

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, &node->mac, sizeof(struct ether_addr)) == 0) {
      memset(current->vid_addr, '\0', VID_ADDR_LEN);
      strncpy(current->vid_addr, node->vid_addr, strlen(node->vid_addr));
      return true;
    }
  }
  return false;
}

/**
 *              Check for link Failures.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool checkForFailuresCPVID() {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  time_t currentTime = time(0);
  bool hasDeletions = false;

  while (current != NULL) {
    if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
      struct child_pvid_tuple *temp = current;
      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }
      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *    Add into the local host broadcast Table.
 *    Local Host broadcast Table, Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/

bool add_entry_lbcast_LL(struct local_bcast_tuple *node) {
  if (local_bcast_head == NULL) {
    local_bcast_head = node;
  } else {
    if (!find_entry_lbcast_LL(node)) {
      node->next = local_bcast_head;
      local_bcast_head = node;
    }
  }
}

/**
 *    Find entries in the local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      If element is found.
 *    false     If element is not found.
**/

bool find_entry_lbcast_LL(struct local_bcast_tuple *node) {
  struct local_bcast_tuple *current = local_bcast_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->eth_name, node->eth_name) == 0) {
        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void      
**/

void print_entries_lbcast_LL() {
  struct local_bcast_tuple *current;


  printf("\n#######Local Host Broadcast Table#########\n");
  printf("PORT\n");

  for (current = local_bcast_head; current != NULL; current = current->next) {
    printf("%s\n", current->eth_name);
  }
}

/**
 *    Delete a port from local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true  - if deletion successful.
 *    false - if deletion not successful.      
**/

bool delete_entry_lbcast_LL(char *port) {
  struct local_bcast_tuple *current = local_bcast_head;
  struct local_bcast_tuple *previous = NULL;
  bool isPortDeleted = false;

  while (current != NULL) {
    if (strcmp(current->eth_name, port) == 0) {
      if (current == local_bcast_head) {
        local_bcast_head = current->next;
        free(current);
        current = local_bcast_head;
        previous = NULL;
        continue;
      } else {
        struct local_bcast_tuple *temp = current;
        current = current->next;
        previous->next = current;
        free(temp);
        continue;
      }
      isPortDeleted = true;
    }
    previous = current;
    current = current->next;
  }
  return isPortDeleted;
}

/**
 *    Return instance of local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    struct local_bcast_tuple* - Returns reference to local host broadcast table.      
**/

struct local_bcast_tuple* getInstance_lbcast_LL() {
  return local_bcast_head;
}
