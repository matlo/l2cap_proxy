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

#include <sched.h>



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

#define SLAVE_CONNECTING_INDEX 3
#define MASTER_CONNECTING_INDEX 4

#define MAX_INDEX 5

#define CID_SLAVE_INDEX 0
#define CID_MASTER_INDEX 1

#define CID_MAX_INDEX 2

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

static void close_fd(struct pollfd* pfd)
{
  close(pfd->fd);
  pfd->fd = -1;
}

int main(int argc, char *argv[])
{
  char* master = NULL;
  char* local = NULL;
  uint32_t device_class = 0x508;
  unsigned char buf[4096];
  ssize_t len;
  int ret;
  unsigned int cpt = 0;
  bdaddr_t bdaddr_a;
  unsigned short psm_a;
  unsigned short cid_a;
  int fd_a;
  bdaddr_t bdaddr_m;
  char slave[sizeof("00:00:00:00:00:00")+1] = {};
  unsigned short cid[CID_MAX_INDEX][PSM_MAX_INDEX];

  /*
   * Set highest priority & scheduler policy.
   */
  struct sched_param p =
  { .sched_priority = sched_get_priority_max(SCHED_FIFO) };

  sched_setscheduler(0, SCHED_FIFO, &p);

  setlinebuf(stdout);

  (void) signal(SIGINT, terminate);

  /* Check args */
  if (argc >= 1)
    master = argv[1];

  if (argc >= 2)
    local = argv[2];

  if (argc >= 3)
    device_class = strtol(argv[3], NULL, 0);

  if (!master || bachk(master) == -1 || (local && bachk(local) == -1)) {
    printf("usage: %s <ps3-mac-address> <dongle-mac-address> <device-class>\n", *argv);
    return 1;
  }

  str2ba(master, &bdaddr_m);

  if(bt_write_device_class(local, device_class) < 0)
  {
    printf("failed to set device class\n");
    return 1;
  }

  /*
   * table 1: fds to accept new connections
   * table 2: fds connected to the slave
   * table 3: fds connected to the master
   *
   * table 4: fds connecting to the slave
   * table 5: fds connecting to the master
   */
  struct pollfd pfd[MAX_INDEX][PSM_MAX_INDEX] = {};

  int i, psm;
  for(i=0; i<MAX_INDEX; ++i)
  {
    for(psm=0; psm<PSM_MAX_INDEX; ++psm)
    {
      pfd[i][psm].fd = -1;
    }
  }

  for(psm=0; psm<PSM_MAX_INDEX; ++psm)
  {
    pfd[LISTEN_INDEX][psm].fd = l2cap_listen(psm_list[psm]);
    pfd[LISTEN_INDEX][psm].events = POLLIN;
  }

  while(!done)
  {
    if(poll(*pfd, MAX_INDEX*PSM_MAX_INDEX, -1))
    {
      for(i=0; i<MAX_INDEX; ++i)
      {
        for(psm=0; psm<PSM_MAX_INDEX; ++psm)
        {
          if (pfd[i][psm].revents & (POLLERR | POLLHUP))
          {
            switch(i)
            {
              case LISTEN_INDEX:
                printf("poll error from listening socket (psm: 0x%04x)\n", psm_list[psm]);
                break;
              case SLAVE_INDEX:
                printf("poll error from SLAVE (psm: 0x%04x)\n", psm_list[psm]);
                close_fd(&pfd[SLAVE_INDEX][psm]);
                close_fd(&pfd[MASTER_INDEX][psm]);
                break;
              case MASTER_INDEX:
                printf("poll error from MASTER (psm: 0x%04x)\n", psm_list[psm]);
                close_fd(&pfd[MASTER_INDEX][psm]);
                close_fd(&pfd[SLAVE_INDEX][psm]);
                break;
            }
          }

          if(pfd[i][psm].revents & POLLOUT)
          {
            if(l2cap_is_connected(pfd[i][psm].fd))
            {
              switch(i)
              {
                case SLAVE_CONNECTING_INDEX:
                  printf("connected to %s (psm: 0x%04x)\n", slave, psm_list[psm]);
                  pfd[SLAVE_INDEX][psm].fd = pfd[i][psm].fd;
                  pfd[SLAVE_INDEX][psm].events = POLLIN;
                  break;
                case MASTER_CONNECTING_INDEX:
                  printf("connected to %s (psm: 0x%04x)\n", master, psm_list[psm]);
                  pfd[MASTER_INDEX][psm].fd = pfd[i][psm].fd;
                  pfd[MASTER_INDEX][psm].events = POLLIN;
                  break;
              }
              pfd[i][psm].fd = -1;
            }
          }

          if(pfd[i][psm].revents & POLLIN)
          {
            switch(i)
            {
              case LISTEN_INDEX:

                fd_a = l2cap_accept(pfd[i][psm].fd, &bdaddr_a, &psm_a, &cid_a);

                if(fd_a < 0)
                {
                  printf("accept error (psm: 0x%04x)\n", psm_list[psm]);
                  break;
                }

                if(bacmp(&bdaddr_a, &bdaddr_m))
                {
                  ba2str(&bdaddr_a, slave);
                  
                  cid[CID_SLAVE_INDEX][psm] = cid_a;

                  if(pfd[SLAVE_INDEX][psm].fd < 0)
                  {
                    pfd[SLAVE_INDEX][psm].fd = fd_a;
                    pfd[SLAVE_INDEX][psm].events = POLLIN;

                    printf("connecting with %s to %s (psm: 0x%04x)\n", local, master, psm_list[psm]);

                    pfd[MASTER_CONNECTING_INDEX][psm].fd = l2cap_connect(local, master, psm_list[psm]);
                    pfd[MASTER_CONNECTING_INDEX][psm].events = POLLOUT;

                    if(pfd[MASTER_CONNECTING_INDEX][psm].fd < 0)
                    {
                      printf("can't start connection to MASTER (psm: 0x%04x)\n", psm_list[psm]);
                      close_fd(&pfd[SLAVE_INDEX][psm]);
                    }
                  }
                  else
                  {
                    close(fd_a);
                    fprintf(stderr, "psm already used: 0x%04x\n", psm_list[psm]);
                  }
                }
                else
                {
                  cid[CID_MASTER_INDEX][psm] = cid_a;

                  if(pfd[MASTER_INDEX][psm].fd < 0)
                  {
                    pfd[MASTER_INDEX][psm].fd = fd_a;
                    pfd[MASTER_INDEX][psm].events = POLLIN;

                    printf("connecting with %s to %s (psm: 0x%04x)\n", local, slave, psm_list[psm]);

                    pfd[SLAVE_CONNECTING_INDEX][psm].fd = l2cap_connect(local, slave, psm_list[psm]);
                    pfd[SLAVE_CONNECTING_INDEX][psm].events = POLLOUT;

                    if(pfd[SLAVE_CONNECTING_INDEX][psm].fd < 0)
                    {
                      printf("can't start connection to SLAVE (psm: 0x%04x)\n", psm_list[psm]);
                      close_fd(&pfd[MASTER_INDEX][psm]);
                    }
                  }
                  else
                  {
                    close(fd_a);
                    fprintf(stderr, "psm already used: 0x%04x\n", psm_list[psm]);
                  }
                }
                break;
              case SLAVE_INDEX:
                if(pfd[MASTER_INDEX][psm].fd < 0)
                {
                  break;
                }
                len = read(pfd[SLAVE_INDEX][psm].fd, buf, sizeof(buf));
                if (len > 0)
                {
                  /*
                   * TODO: send acl packets when packet size is larger than the outgoing MTU size
                   */
                  ret = write(pfd[MASTER_INDEX][psm].fd, buf, len);
                  if(ret < 0)
                  {
                    printf("write error (SLAVE > MASTER) (psm: 0x%04x)\n", psm_list[psm]);
                  }
                  if(debug)
                  {
                    printf("SLAVE > MASTER (psm: 0x%04x)\n", psm_list[psm]);
                    dump(buf, len);
                  }
                }
                else if(errno != EINTR)
                {
                  printf("recv error from SLAVE (psm: 0x%04x)\n", psm_list[psm]);
                  close_fd(&pfd[SLAVE_INDEX][psm]);
                  close_fd(&pfd[MASTER_INDEX][psm]);
                }
                else
                {
                  printf("write interrupted (SLAVE > MASTER) (psm: 0x%04x)\n", psm_list[psm]);
                }
                break;
              case MASTER_INDEX:
                if(pfd[SLAVE_INDEX][psm].fd < 0)
                {
                  break;
                }
                len = read(pfd[MASTER_INDEX][psm].fd, buf, sizeof(buf));
                if (len > 0)
                {
                  /*
                   * TODO: send acl packets when packet size is larger than the outgoing MTU size
                   */
                  ret = write(pfd[SLAVE_INDEX][psm].fd, buf, len);
                  if(ret < 0)
                  {
                    printf("write error (MASTER > SLAVE) (psm: 0x%04x)\n", psm_list[psm]);
                  }
                  if(debug)
                  {
                    printf("MASTER > SLAVE (psm: 0x%04x)\n", psm_list[psm]);
                    dump(buf, len);
                  }
                }
                else if(errno != EINTR)
                {
                  printf("recv error from server (psm: 0x%04x)\n", psm_list[psm]);
                  close_fd(&pfd[MASTER_INDEX][psm]);
                  close_fd(&pfd[SLAVE_INDEX][psm]);
                }
                else
                {
                  printf("write interrupted (MASTER > SLAVE) (psm: 0x%04x)\n", psm_list[psm]);
                }
                break;
            }
          }
        }
      }
    }
  }

  for(i=0; i<MAX_INDEX; ++i)
  {
    for(psm=0; psm<PSM_MAX_INDEX; ++psm)
    {
      close(pfd[i][psm].fd);
    }
  }

  return 0;
}
