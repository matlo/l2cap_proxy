/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

int l2cap_connect(const char*, int);

int l2cap_send(int, const unsigned char*, int, int);

int l2cap_recv(int, unsigned char*, int);

int l2cap_listen(int);

int l2cap_accept(int);

#ifdef WIN32
int bachk(const char *str);
#endif
