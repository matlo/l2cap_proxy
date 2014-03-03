/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#ifndef L2CAP_CON_H_
#define L2CAP_CON_H_

#include <bluetooth/bluetooth.h>

int l2cap_connect(const char*, const char*, int);

int l2cap_is_connected(int fd);

int l2cap_send(int, const unsigned char*, int, int);

int l2cap_recv(int, unsigned char*, int);

int l2cap_listen(int);

int l2cap_accept(int s, bdaddr_t* src, unsigned short* psm, unsigned short* cid);

#endif
