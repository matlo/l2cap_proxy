/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#ifndef L2CAP_CON_H_
#define L2CAP_CON_H_

int l2cap_connect(const char*, const char*, int);

int l2cap_send(int, const unsigned char*, int, int);

int l2cap_recv(int, unsigned char*, int);

int l2cap_listen(int);

int l2cap_accept(int);

#endif
