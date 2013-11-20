/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "l2cap_con.h"
#include <sys/time.h>
#include <signal.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <bluetooth/bluetooth.h>
#include <sys/types.h>
#include "bt_utils.h"

#define CTRL 17
#define DATA 19

static int debug_data = 0;
static int debug_ctrl = 0;

static volatile int done = 0;

void terminate(int sig)
{
  done = 1;
}

void dump(unsigned char* buf, int len)
{
  int i;
  for(i=0; i<len; ++i)
  {
    printf("0x%02x ", buf[i]);
    if(!((i+1)%8))
    {
      printf("\n");
    }
  }
  printf("\n");
}

int main(int argc, char *argv[])
{
	int ctrl_fd, data_fd;
	int client_ctrl, client_data;
	int server_ctrl, server_data;
	char* bdaddr;
	fd_set read_set;
	unsigned char buf[1024];
	ssize_t len;
	int fd_max = 0;
  struct timeval tv;

  (void) signal(SIGINT, terminate);

	/* Check args */
	if (argc >= 1)
		bdaddr = argv[1];

	if (bachk(bdaddr) == -1) {
		printf("usage: %s <ps3-mac-address>\n", *argv);
		return 1;
	}

	if(write_device_class(0, 0x508) < 0)
	{
    printf("failed to set device class\n");
    return 1;
	}

	ctrl_fd = l2cap_listen(CTRL);
	client_ctrl = l2cap_accept(ctrl_fd);

	data_fd = l2cap_listen(DATA);
	client_data = l2cap_accept(data_fd);

	server_ctrl = l2cap_connect(bdaddr, CTRL);
	server_data = l2cap_connect(bdaddr, DATA);

	if(client_ctrl > fd_max)
	{
	  fd_max = client_ctrl;
	}

  if(client_data > fd_max)
  {
    fd_max = client_data;
  }

  if(server_ctrl > fd_max)
  {
    fd_max = server_ctrl;
  }

  if(server_data > fd_max)
  {
    fd_max = server_data;
  }

	while(!done)
	{
		FD_ZERO(&read_set);
		FD_SET(client_ctrl, &read_set);
		FD_SET(client_data, &read_set);
		FD_SET(server_ctrl, &read_set);
		FD_SET(server_data, &read_set);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		select(fd_max+1, &read_set, NULL, NULL, &tv);

		if (FD_ISSET(client_ctrl, &read_set))
		{
			len = recv(client_ctrl, buf, 1024, MSG_DONTWAIT);
			if (len > 0)
			{
				send(server_ctrl, buf, len, MSG_DONTWAIT);
				if(debug_ctrl)
				{
          printf("CTRL CLIENT > SERVER\n");
          dump(buf, len);
				}
			}
			else
			{
			  printf("connection error from client (control)\n");
			  done = 1;
			}
		}

		if (FD_ISSET(client_data, &read_set))
		{
			len = recv(client_data, buf, 1024, MSG_DONTWAIT);
			if (len > 0)
			{
				send(server_data, buf, len, MSG_DONTWAIT);
        if(debug_data)
        {
          printf("DATA CLIENT > SERVER\n");
          dump(buf, len);
        }
			}
      else
      {
        printf("connection error from client (data)\n");
        done = 1;
      }
		}

		if (FD_ISSET(server_ctrl, &read_set))
		{
			len = recv(server_ctrl, buf, 1024, MSG_DONTWAIT);
			if (len > 0)
			{
				send(client_ctrl, buf, len, MSG_DONTWAIT);
        if(debug_ctrl)
        {
          printf("CTRL SERVER > CLIENT\n");
          dump(buf, len);
        }
			}
      else
      {
        printf("connection error from server (control)\n");
        done = 1;
      }
		}

		if (FD_ISSET(server_data, &read_set))
		{
			len = recv(server_data, buf, 1024, MSG_DONTWAIT);
			if (len > 0)
			{
				send(client_data, buf, len, MSG_DONTWAIT);
        if(debug_data)
        {
          printf("DATA SERVER > CLIENT\n");
          dump(buf, len);
        }
			}
      else
      {
        printf("connection error from server (data)\n");
        done = 1;
      }
		}
	}

  close(server_data);
  printf("%s\n", strerror(errno));
  close(server_ctrl);
  printf("%s\n", strerror(errno));

  close(client_data);
  printf("%s\n", strerror(errno));
  close(client_ctrl);
  printf("%s\n", strerror(errno));

  close(data_fd);
  close(ctrl_fd);

	return 0;
}
