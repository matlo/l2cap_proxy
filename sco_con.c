/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/sco.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "bt_utils.h"

#ifdef BT_POWER
#warning "BT_POWER is already defined."
#else
#define BT_POWER 9
#define BT_POWER_FORCE_ACTIVE_OFF 0
#define BT_POWER_FORCE_ACTIVE_ON  1

struct bt_power {
  unsigned char force_active;
};
#endif

int sco_listen(unsigned short psm)
{
  struct sockaddr_sco loc_addr = { 0 };
  int s;

  // allocate socket
  if((s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO)) < 0)
  {
    perror("socket");
    exit(-1);
  }

  loc_addr.sco_family = AF_BLUETOOTH;
  loc_addr.sco_bdaddr = *BDADDR_ANY;

  if(bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) < 0)
  {
    perror("bind");
    exit(-1);
  }

  int defer_setup = 1;
  if(setsockopt(s, SOL_BLUETOOTH, BT_DEFER_SETUP, &defer_setup, sizeof(defer_setup)) < 0)
  {
    perror("setsockopt BT_DEFER_SETUP");
    exit(-1);
  }

  // put socket into listening mode
  if(listen(s, 10) < 0)
  {
    perror("listen");
    exit(-1);
  }

  printf("listening on psm: 0x%04x\n", psm);

  return s;
}

int sco_accept(int s)
{
  struct sockaddr_sco rem_addr = { 0 };
  int client;
  char buf[sizeof("00:00:00:00:00:00")+1] = { 0 };
  socklen_t opt = sizeof(rem_addr);

  // accept one connection
  if((client = accept(s, (struct sockaddr *) &rem_addr, &opt)) < 0)
  {
    perror("accept");
    exit(-1);
  }

  ba2str(&rem_addr.sco_bdaddr, buf);
  printf("accepted connection from %s (SCO)\n", buf);

  //TODO: Is this needed?
  struct bt_power pwr = {.force_active = BT_POWER_FORCE_ACTIVE_OFF};
  if (setsockopt(client, SOL_BLUETOOTH, BT_POWER, &pwr, sizeof(pwr)) < 0)
  {
    perror("setsockopt BT_POWER");
  }

  return client;
}

int sco_connect(const char *bdaddr_src, const char *bdaddr_dest)
{
  int fd;
  struct sockaddr_sco addr;

  if ((fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO)) < 0)
  {
    perror("socket");
    exit(-1);
  }

  if(bdaddr_src)
  {
    memset(&addr, 0, sizeof(addr));
    addr.sco_family = AF_BLUETOOTH;
    str2ba(bdaddr_src, &addr.sco_bdaddr);
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      perror("bind\n");
      exit(-1);
    }
  }

  memset(&addr, 0, sizeof(addr));
  addr.sco_family = AF_BLUETOOTH;
  str2ba(bdaddr_dest, &addr.sco_bdaddr);
  if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
  {
    perror("connect");
    exit(-1);
  }
  
  printf("connected to %s (SCO)\n", bdaddr_dest);

  //TODO: Is this needed?
  struct bt_power pwr = {.force_active = BT_POWER_FORCE_ACTIVE_OFF};
  if (setsockopt(fd, SOL_BLUETOOTH, BT_POWER, &pwr, sizeof(pwr)) < 0)
  {
    perror("setsockopt BT_POWER");
  }
  
  return fd;
}
