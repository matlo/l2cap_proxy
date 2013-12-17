/*
 Copyright (c) 2010 Mathieu Laurendeau <mat.lau@laposte.net>
   License: GPLv3
*/

#ifndef BT_UTILS_H_
#define BT_UTILS_H_

int get_device_bdaddr(int device_number, char bdaddr[18]);
int write_device_class(char* bdaddr, uint32_t class);

int delete_stored_link_key(char* bdaddr, char* bdaddr_dest);
int write_stored_link_key(char* bdaddr, char* bdaddr_dest, unsigned char* key);
int authenticate_link(char* bdaddr_dest);
int encrypt_link(char* bdaddr_dest);

#endif /* BT_UTILS_H_ */
