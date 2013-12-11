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

/*
 * https://www.bluetooth.org/en-us/specification/assigned-numbers/logical-link-control
 */
#define PSM_SDP 0x0001 //Service Discovery Protocol
#define PSM_RFCOMM  0x0003 //Can't be used for L2CAP sockets
#define PSM_TCS_BIN 0x0005 //Telephony Control Specification
#define PSM_TCS_BIN_CORDLESS  0x0007 //Telephony Control Specification
#define PSM_BNEP  0x000F //Bluetooth Network Encapsulation Protocol
#define PSM_HID_Control 0x0011 //Human Interface Device
#define PSM_HID_Interrupt 0x0013 //Human Interface Device
#define PSM_UPnP  0x0015
#define PSM_AVCTP 0x0017 //Audio/Video Control Transport Protocol
#define PSM_AVDTP 0x0019 //Audio/Video Distribution Transport Protocol
#define PSM_AVCTP_Browsing  0x001B //Audio/Video Remote Control Profile
#define PSM_UDI_C_Plane 0x001D //Unrestricted Digital Information Profile
#define PSM_ATT 0x001F
#define PSM_3DSP 0x0021 //3D Synchronization Profile

static unsigned short psm_list[] =
{
    PSM_SDP,
    PSM_TCS_BIN,
    PSM_TCS_BIN_CORDLESS,
    PSM_BNEP,
    PSM_HID_Control,
    PSM_HID_Interrupt,
    PSM_UPnP,
    PSM_AVCTP,
    PSM_AVDTP,
    PSM_AVCTP_Browsing,
    PSM_UDI_C_Plane,
    PSM_ATT,
    PSM_3DSP
};

#define PSM_MAX_INDEX (sizeof(psm_list)/sizeof(*psm_list))

#define MAX_FDS (PSM_MAX_INDEX*3)

#define SLAVE_OFFSET (MAX_FDS/3)
#define MASTER_OFFSET (2*MAX_FDS/3)

static int debug = 0;

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
  char* master = NULL;
  char* slave = NULL;
  uint32_t device_class = 0x508;
  unsigned char buf[1024];
  ssize_t len;

  (void) signal(SIGINT, terminate);

  /* Check args */
  if (argc >= 1)
    master = argv[1];

  if (argc >= 2)
    slave = argv[2];

  if (argc >= 3)
    device_class = strtol(argv[3], NULL, 0);

  if (!master || bachk(master) == -1 || (slave && bachk(slave) == -1)) {
    printf("usage: %s <ps3-mac-address> <dongle-mac-address> <device-class>\n", *argv);
    return 1;
  }

  if(write_device_class(0, device_class) < 0)
  {
    printf("failed to set device class\n");
    return 1;
  }

  /*
   * [0..MAX_FDS/3[            fds to accept new connections
   * [MAX_FDS/3..2*MAX_FDS/3[  fds connected to the slave
   * [2*MAX_FDS/3..MAX_FDS[    fds connected to the master
   */
  struct pollfd pfd[MAX_FDS] = {};

  int i;
  for(i=0; i<MAX_FDS; ++i)
  {
    pfd[i].fd = -1;
  }

  for(i=0; i<PSM_MAX_INDEX; ++i)
  {
    pfd[i].fd = l2cap_listen(psm_list[i]);
  }

  while(!done)
  {
    for(i=0; i<MAX_FDS; ++i)
    {
      if(pfd[i].fd > -1)
      {
        pfd[i].events = POLLIN;
      }
    }

    if(poll(pfd, MAX_FDS, -1))
    {
      for(i=0; i<MAX_FDS; ++i)
      {
        if (pfd[i].revents & POLLERR)
        {
          if(i < SLAVE_OFFSET)
          {
            printf("connection error from listening socket (psm: 0x%04x)\n", psm_list[i]);
          }
          else if(i >= SLAVE_OFFSET && i < MASTER_OFFSET)
          {
            printf("connection error from client (psm: 0x%04x)\n", psm_list[i-SLAVE_OFFSET]);
            close(pfd[i].fd);
            pfd[i].fd = -1;
            close(pfd[i+SLAVE_OFFSET].fd);
            pfd[i+SLAVE_OFFSET].fd = -1;
          }
          else
          {
            printf("connection error from server (psm: 0x%04x)\n", psm_list[i-MASTER_OFFSET]);
            close(pfd[i].fd);
            pfd[i].fd = -1;
            close(pfd[i-SLAVE_OFFSET].fd);
            pfd[i-SLAVE_OFFSET].fd = -1;
          }
        }

        if(pfd[i].revents & POLLIN)
        {
          if(i < SLAVE_OFFSET)
          {
            if(pfd[i+SLAVE_OFFSET].fd < 0)
            {
              pfd[i+SLAVE_OFFSET].fd = l2cap_accept(pfd[i].fd);

              if(pfd[i+SLAVE_OFFSET].fd > -1)
              {
                pfd[i+MASTER_OFFSET].fd = l2cap_connect(slave, master, psm_list[i]);

                if(pfd[i+MASTER_OFFSET].fd < 0)
                {
                  printf("can't connect to server (psm: 0x%04x)\n", psm_list[i]);
                  close(pfd[i+SLAVE_OFFSET].fd);
                  pfd[i+SLAVE_OFFSET].fd = -1;
                }
              }
              else
              {
                printf("connection error from client (psm: 0x%04x)\n", psm_list[i]);
              }
            }
            else
            {
              fprintf(stderr, "psm already used: 0x%04x\n", psm_list[i]);
            }
          }
          else if(i >= SLAVE_OFFSET && i < MASTER_OFFSET)
          {
            len = recv(pfd[i].fd, buf, 1024, MSG_DONTWAIT);
            if (len > 0)
            {
              send(pfd[i+MAX_FDS/3].fd, buf, len, MSG_DONTWAIT);
              if(debug)
              {
                printf("CLIENT > SERVER (psm: 0x%04x)\n", psm_list[i-SLAVE_OFFSET]);
                dump(buf, len);
              }
            }
            else if(errno != EINTR)
            {
              printf("connection error from client (psm: 0x%04x)\n", psm_list[i-SLAVE_OFFSET]);
              close(pfd[i].fd);
              pfd[i].fd = -1;
              close(pfd[i+SLAVE_OFFSET].fd);
              pfd[i+SLAVE_OFFSET].fd = -1;
            }
          }
          else
          {
            len = recv(pfd[i].fd, buf, 1024, MSG_DONTWAIT);
            if (len > 0)
            {
              send(pfd[i-SLAVE_OFFSET].fd, buf, len, MSG_DONTWAIT);
              if(debug)
              {
                printf("SERVER > CLIENT (psm: 0x%04x)\n", psm_list[i-MASTER_OFFSET]);
                dump(buf, len);
              }
            }
            else if(errno != EINTR)
            {
              printf("connection error from server (psm: 0x%04x)\n", psm_list[i-MASTER_OFFSET]);
              close(pfd[i].fd);
              pfd[i].fd = -1;
              close(pfd[i-SLAVE_OFFSET].fd);
              pfd[i-SLAVE_OFFSET].fd = -1;
            }
          }
        }
      }
    }
  }

  for(i=MAX_FDS-1; i>-1; --i)
  {
    if(pfd[i].fd > -1)
    {
      close(pfd[i].fd);
    }
  }

  return 0;
}
