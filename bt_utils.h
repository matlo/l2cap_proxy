/*
 Copyright (c) 2010 Mathieu Laurendeau <mat.lau@laposte.net>
   License: GPLv3
*/

#ifndef BT_UTILS_H_
#define BT_UTILS_H_

int get_device_bdaddr(int, char*);
int write_device_class(int, uint32_t);

int delete_stored_link_key(int device_number, char* bdaddr);
int write_stored_link_key(int device_number, char* bdaddr, unsigned char* key);
int authenticate_link(char* bdaddr);
int encrypt_link(char* bdaddr);

#endif /* BT_UTILS_H_ */
