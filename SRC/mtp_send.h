/*
 *  mtp_send.h
 *
 */

#ifndef MTP_SEND_H
#define MTP_SEND_H

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

// Destination MAC address (Presently broadcast address)
#define MY_DEST_MAC0	0xFF
#define MY_DEST_MAC1	0xFF
#define MY_DEST_MAC2	0xFF
#define MY_DEST_MAC3	0xFF
#define MY_DEST_MAC4	0xFF
#define MY_DEST_MAC5	0xFF

// Allocating size to different containers
#define HEADER_SIZE		14

/* Function Prototypes */
int ctrlSend(char *, uint8_t *, int);  
int dataSend(char *, uint8_t *, int);

#endif
