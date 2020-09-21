/*
 Copyright (c) 2013 Mathieu Laurendeau
 License: GPLv3
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <unistd.h>
#include "bt_utils.h"
#include <errno.h>
#include <stdlib.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

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

#define ACL_MTU 1024

/*
 * This function can be used to bypass the l2cap outgoing MTU check of the Linux kernel.
 * If plen is higher than ACL_MTU, it sends a segmented packet.
 */
int acl_send_data (const char *bdaddr_dst, unsigned short cid, const unsigned char *data, unsigned short plen)
{
  int ret = -1, dd, device;
  struct hci_conn_info_req *cr = 0;
  bdaddr_t ba;
  uint8_t type = HCI_ACLDATA_PKT;
  hci_acl_hdr acl_hdr;
  l2cap_hdr l2_hdr;
  struct iovec iv[4];
  int ivn;
  unsigned short data_len;
  char* pdata = (char*)data;

  str2ba(bdaddr_dst, &ba);

  // find the connection handle to the specified bluetooth device
  cr = (struct hci_conn_info_req*) malloc(
      sizeof(struct hci_conn_info_req) + sizeof(struct hci_conn_info));
  bacpy(&cr->bdaddr, &ba);
  cr->type = ACL_LINK;

  if ((device = hci_get_route(&ba)) < 0)
  {
    perror("hci_get_route");
    goto cleanup;
  }

  if ((dd = hci_open_dev(device)) < 0)
  {
    perror("hci_open_dev");
    goto cleanup;
  }

  if (ioctl(dd, HCIGETCONNINFO, (unsigned long) cr) < 0)
  {
    perror("ioctl HCIGETCONNINFO");
    goto cleanup;
  }
  
  data_len = ACL_MTU-1-HCI_ACL_HDR_SIZE-L2CAP_HDR_SIZE;
  if(plen < data_len)
  {
    data_len = plen;
  }

  iv[0].iov_base = &type;
  iv[0].iov_len = 1;

  acl_hdr.handle = htobs(acl_handle_pack(cr->conn_info->handle, ACL_START));
  acl_hdr.dlen = htobs(data_len+L2CAP_HDR_SIZE);
  
  iv[1].iov_base = &acl_hdr;
  iv[1].iov_len = HCI_ACL_HDR_SIZE;

  l2_hdr.cid = htobs(cid);
  l2_hdr.len = htobs(plen);

  iv[2].iov_base = &l2_hdr;
  iv[2].iov_len = L2CAP_HDR_SIZE;

  ivn = 3;
  
  if (data_len)
  {
    iv[3].iov_base = pdata;
    iv[3].iov_len = htobs(data_len);
    ivn = 4;
  }
  
  while ((ret = writev(dd, iv, ivn)) < 0)
  {
    if (errno == EAGAIN || errno == EINTR)
    {
      continue;
    }
    perror("writev: ");
    break;
  }
  
  plen -= data_len;
  
  while(plen)
  {
    pdata += data_len;
    data_len = ACL_MTU-1-HCI_ACL_HDR_SIZE;
    if(plen < data_len)
    {
      data_len = plen;
    }

    iv[0].iov_base = &type;
    iv[0].iov_len = 1;

    acl_hdr.handle = htobs(acl_handle_pack(cr->conn_info->handle, ACL_CONT));
    acl_hdr.dlen = htobs(plen);

    iv[1].iov_base = &acl_hdr;
    iv[1].iov_len = HCI_ACL_HDR_SIZE;

    iv[2].iov_base = pdata;
    iv[2].iov_len = htobs(data_len);
    ivn = 3;
  
    while ((ret = writev(dd, iv, ivn)) < 0)
    {
      if (errno == EAGAIN || errno == EINTR)
      {
        continue;
      }
      perror("writev: ");
      break;
    }
    
    plen -= data_len;
  }
  
  cleanup: free(cr);
  if (dd >= 0)
    close(dd);
  return ret;
}

#define L2CAP_MTU 1024

static void l2cap_setsockopt(int fd)
{
  struct l2cap_options l2o;
  socklen_t len = sizeof(l2o);

  int opt = L2CAP_LM_MASTER;
  if (setsockopt(fd, SOL_L2CAP, L2CAP_LM, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt L2CAP_LM");
  }

  memset(&l2o, 0, sizeof(l2o));
  if(getsockopt(fd, SOL_L2CAP, L2CAP_OPTIONS, &l2o, &len) < 0)
  {
    perror("getsockopt L2CAP_OPTIONS");
  }
  else
  {
    l2o.omtu = L2CAP_MTU;
    l2o.imtu = L2CAP_MTU;
    if(setsockopt(fd, SOL_L2CAP, L2CAP_OPTIONS, &l2o, sizeof(l2o)) < 0)
    {
      perror("setsockopt L2CAP_OPTIONS");
    }
  }

  /*if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
  {
    perror("fcntl O_NONBLOCK");
  }*/

  struct bt_power pwr = {.force_active = BT_POWER_FORCE_ACTIVE_OFF};
  if (setsockopt(fd, SOL_BLUETOOTH, BT_POWER, &pwr, sizeof(pwr)) < 0)
  {
    perror("setsockopt BT_POWER");
  }
}

int l2cap_connect(const char *bdaddr_src, const char *bdaddr_dest, int psm)
{
    int fd;
    struct sockaddr_l2 addr;
    //int opt;

    if ((fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET | SOCK_NONBLOCK, BTPROTO_L2CAP)) == -1)
    {
      perror("socket");
      return -1;
    }

    /*opt = 0;
    if (setsockopt(fd, SOL_L2CAP, L2CAP_LM, &opt, sizeof(opt)) < 0)
    {
      perror("setsockopt L2CAP_LM");
      close(fd);
      return -3;
    }*/

    l2cap_setsockopt(fd);

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    str2ba(bdaddr_src, &addr.l2_bdaddr);
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm    = htobs(psm);
    str2ba(bdaddr_dest, &addr.l2_bdaddr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
      if(errno != EINPROGRESS)
      {
        perror("connect");
        close(fd);
        return -4;
      }
    }

    return fd;
}

int l2cap_send(const char* bdaddr_dst, unsigned short cid, int fd, const unsigned char* buf, int len)
{
  if(len > L2CAP_DEFAULT_MTU)
  {
    if(acl_send_data(bdaddr_dst, cid, buf, len) < 0)
    {
      perror("acl_send_data");
      return -1;
    }
  }
  else
  {
    if(write(fd, buf, len) != len)
    {
      perror("write");
      return -1;
    }
  }
  return len;
}

int l2cap_recv(int fd, unsigned char* buf, int len)
{
    return recv(fd, buf, len, MSG_DONTWAIT);
}

int l2cap_listen(unsigned short psm)
{
  struct sockaddr_l2 loc_addr = { 0 };
  int s;

  // allocate socket
  if((s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
  {
    perror("socket");
    return -1;
  }

  l2cap_setsockopt(s);

  // bind socket to port psm of the first available
  // bluetooth adapter
  loc_addr.l2_family = AF_BLUETOOTH;
  loc_addr.l2_bdaddr = *BDADDR_ANY;
  loc_addr.l2_psm = htobs(psm);

  if(bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) < 0)
  {
    perror("bind");
    close(s);
    return -1;
  }

  // put socket into listening mode
  if(listen(s, 10) < 0)
  {
    perror("listen");
    close(s);
    return -1;
  }

  printf("listening on psm: 0x%04x\n", psm);

  return s;
}

int l2cap_accept(int s, bdaddr_t* src, unsigned short* psm, unsigned short* cid)
{
  struct sockaddr_l2 rem_addr = { 0 };
  int client;
  char buf[sizeof("00:00:00:00:00:00")+1] = { 0 };
  socklen_t opt = sizeof(rem_addr);

  // accept one connection
  if((client = accept(s, (struct sockaddr *) &rem_addr, &opt)) < 0)
  {
    perror("accept");
    return -1;
  }

  ba2str(&rem_addr.l2_bdaddr, buf);
  printf("accepted connection from %s (psm: 0x%04x)\n", buf, btohs(rem_addr.l2_psm));
  
  bacpy(src, &rem_addr.l2_bdaddr);
  *psm = btohs(rem_addr.l2_psm);
  *cid = btohs(rem_addr.l2_cid);

  return client;
}

int l2cap_is_connected(int fd)
{
  int error = 0;
  socklen_t lerror = sizeof(error);

  int ret = getsockopt (fd, SOL_SOCKET, SO_ERROR, &error, &lerror);

  if(ret < 0)
  {
    perror("getsockopt SO_ERROR");
  }
  else
  {
    if(error == EINPROGRESS)
    {
      fprintf(stderr, "EINPROGRESS\n");
    }
    else if(error)
    {
      fprintf(stderr, "connection failed: %s\n", strerror(error));
    }
    else
    {
      return 1;
    }
  }
  return 0;
}
