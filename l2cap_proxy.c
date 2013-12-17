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

#define PS4_TWEAKS

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

#define LISTEN_INDEX 0
#define SLAVE_INDEX  1
#define MASTER_INDEX 2

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

  if(write_device_class(slave, device_class) < 0)
  {
    printf("failed to set device class\n");
    return 1;
  }

#ifdef PS4_TWEAKS
  if(delete_stored_link_key(slave, master) < 0)
  {
    printf("failed to delete stored link key\n");
    return -1;
  }
#endif

  /*
   * table 1: fds to accept new connections
   * table 2: fds connected to the slave
   * table 3: fds connected to the master
   */
  struct pollfd pfd[3][PSM_MAX_INDEX] = {};

  int i, psm;
  for(i=0; i<3; ++i)
  {
    for(psm=0; psm<PSM_MAX_INDEX; ++psm)
    {
      pfd[i][psm].fd = -1;
    }
  }

  for(psm=0; psm<PSM_MAX_INDEX; ++psm)
  {
    pfd[LISTEN_INDEX][psm].fd = l2cap_listen(psm_list[psm]);
  }

  while(!done)
  {
    for(i=0; i<3; ++i)
    {
      for(psm=0; psm<PSM_MAX_INDEX; ++psm)
      {
        if(pfd[i][psm].fd > -1)
        {
          pfd[i][psm].events = POLLIN;
        }
      }
    }

    if(poll(*pfd, 3*PSM_MAX_INDEX, -1))
    {
      for(i=0; i<3; ++i)
      {
        for(psm=0; psm<PSM_MAX_INDEX; ++psm)
        {
          if (pfd[i][psm].revents & POLLERR)
          {
            switch(i)
            {
              case LISTEN_INDEX:
                printf("connection error from listening socket (psm: 0x%04x)\n", psm_list[psm]);
                break;
              case SLAVE_INDEX:
                printf("connection error from client (psm: 0x%04x)\n", psm_list[psm]);
                close(pfd[SLAVE_INDEX][psm].fd);
                pfd[SLAVE_INDEX][psm].fd = -1;
                close(pfd[MASTER_INDEX][psm].fd);
                pfd[MASTER_INDEX][psm].fd = -1;

#ifdef PS4_TWEAKS
                if(psm_list[psm] == PSM_SDP)
                {
                  unsigned char lk[16] =
                  {
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                  };
                  if(write_stored_link_key(slave, master, lk) < 0)
                  {
                    printf("failed to write stored link key\n");
                    done = 1;
                  }
                }
#endif

                break;
              case MASTER_INDEX:
                printf("connection error from server (psm: 0x%04x)\n", psm_list[psm]);
                close(pfd[MASTER_INDEX][psm].fd);
                pfd[MASTER_INDEX][psm].fd = -1;
                close(pfd[SLAVE_INDEX][psm].fd);
                pfd[SLAVE_INDEX][psm].fd = -1;
                break;
            }
          }

          if(pfd[i][psm].revents & POLLIN)
          {
            switch(i)
            {
              case LISTEN_INDEX:
                if(pfd[SLAVE_INDEX][psm].fd < 0)
                {
#ifdef PS4_TWEAKS
                  if(psm_list[psm] == PSM_HID_Control)
                  {
                    if(authenticate_link(master) < 0)
                    {
                      printf("failed to authenticate link\n");
                      done = 1;
                    }
                    if(encrypt_link(master) < 0)
                    {
                      printf("failed to encrypt link\n");
                      done = 1;
                    }
                  }
#endif

                  pfd[SLAVE_INDEX][psm].fd = l2cap_accept(pfd[i][psm].fd);

                  if(pfd[SLAVE_INDEX][psm].fd > -1)
                  {
                    pfd[MASTER_INDEX][psm].fd = l2cap_connect(slave, master, psm_list[psm]);

                    if(pfd[MASTER_INDEX][psm].fd < 0)
                    {
                      printf("can't connect to server (psm: 0x%04x)\n", psm_list[psm]);
                      close(pfd[SLAVE_INDEX][psm].fd);
                      pfd[SLAVE_INDEX][psm].fd = -1;
                    }
                  }
                  else
                  {
                    printf("connection error from client (psm: 0x%04x)\n", psm_list[psm]);
                  }
                }
                else
                {
                  fprintf(stderr, "psm already used: 0x%04x\n", psm_list[psm]);
                }
                break;
              case SLAVE_INDEX:
                len = recv(pfd[SLAVE_INDEX][psm].fd, buf, 1024, MSG_DONTWAIT);
                if (len > 0)
                {
                  send(pfd[MASTER_INDEX][psm].fd, buf, len, MSG_DONTWAIT);
                  if(debug)
                  {
                    printf("CLIENT > SERVER (psm: 0x%04x)\n", psm_list[psm]);
                    dump(buf, len);
                  }
                }
                else if(errno != EINTR)
                {
                  printf("connection error from client (psm: 0x%04x)\n", psm_list[psm]);
                  close(pfd[SLAVE_INDEX][psm].fd);
                  pfd[SLAVE_INDEX][psm].fd = -1;
                  close(pfd[MASTER_INDEX][psm].fd);
                  pfd[MASTER_INDEX][psm].fd = -1;
                }
                break;
              case MASTER_INDEX:
                len = recv(pfd[MASTER_INDEX][psm].fd, buf, 1024, MSG_DONTWAIT);
                if (len > 0)
                {
                  send(pfd[SLAVE_INDEX][psm].fd, buf, len, MSG_DONTWAIT);
                  if(debug)
                  {
                    printf("SERVER > CLIENT (psm: 0x%04x)\n", psm_list[psm]);
                    dump(buf, len);
                  }
                }
                else if(errno != EINTR)
                {
                  printf("connection error from server (psm: 0x%04x)\n", psm_list[psm]);
                  close(pfd[MASTER_INDEX][psm].fd);
                  pfd[MASTER_INDEX][psm].fd = -1;
                  close(pfd[SLAVE_INDEX][psm].fd);
                  pfd[SLAVE_INDEX][psm].fd = -1;
                }
                break;
            }
          }
        }
      }
    }
  }

  for(i=0; i<3; ++i)
  {
    for(psm=0; psm<PSM_MAX_INDEX; ++psm)
    {
      close(pfd[i][psm].fd);
    }
  }

  return 0;
}
