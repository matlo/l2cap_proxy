/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#ifndef SCO_CON_H_
#define SCO_CON_H_

int sco_connect(const char*, const char*, int);

int sco_send(int, const unsigned char*, int, int);

int sco_recv(int, unsigned char*, int);

int sco_listen(int);

int sco_accept(int);

#endif
