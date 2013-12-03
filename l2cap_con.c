/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

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

int l2cap_listen(int psm)
{
	struct sockaddr_l2 loc_addr = { 0 };
	int s;

	// allocate socket
	if((s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
	{
    perror("socket");
    exit(-1);
  }

	// bind socket to port psm of the first available
	// bluetooth adapter
	loc_addr.l2_family = AF_BLUETOOTH;
	loc_addr.l2_bdaddr = *BDADDR_ANY;
	loc_addr.l2_psm = htobs(psm);

	if(bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) < 0)
	{
	  perror("bind");
	  exit(-1);
	}

	// put socket into listening mode
	if(listen(s, 1) < 0)
	{
    perror("listen");
    exit(-1);
  }

	printf("listening on port: %d\n", psm);

	return s;
}

int l2cap_accept(int s)
{
	struct sockaddr_l2 rem_addr = { 0 };
	int client;
	char buf[sizeof("00:00:00:00:00:00")+1] = { 0 };
	socklen_t opt = sizeof(rem_addr);

	// accept one connection
	if((client = accept(s, (struct sockaddr *) &rem_addr, &opt)) < 0)
	{
    perror("accept");
    exit(-1);
  }

	ba2str(&rem_addr.l2_bdaddr, buf);
	printf("accepted connection from %s\n", buf);
  
  struct bt_power pwr = {.force_active = BT_POWER_FORCE_ACTIVE_OFF};
  if (setsockopt(client, SOL_BLUETOOTH, BT_POWER, &pwr, sizeof(pwr)) < 0)
  {
    perror("setsockopt BT_POWER");
  }

	return client;
}

int l2cap_connect(const char *bdaddr, int psm)
{
  int fd;
  struct sockaddr_l2 addr;
  int opt;

  if ((fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
  {
    perror("socket");
    exit(-1);
  }
  opt = 0;
  if (setsockopt(fd, SOL_L2CAP, L2CAP_LM, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt L2CAP_LM");
    exit(-1);
  }
  memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = htobs(psm);
  str2ba(bdaddr, &addr.l2_bdaddr);
  if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
  {
    perror("connect");
    exit(-1);
  }
  
  struct bt_power pwr = {.force_active = BT_POWER_FORCE_ACTIVE_OFF};
  if (setsockopt(fd, SOL_BLUETOOTH, BT_POWER, &pwr, sizeof(pwr)) < 0)
  {
    perror("setsockopt BT_POWER");
  }
  
  return fd;
}
