/*
 Copyright (c) 2010 Mathieu Laurendeau <mat.lau@laposte.net>
   License: GPLv3
*/

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define HCI_REQ_TIMEOUT   1000

/*
 * \brief This function gets the bluetooth device address for a given device number.
 *
 * \param device_number  the device number
 * \param bdaddr         the buffer to store the bluetooth device address
 *
 * \return 0 if successful, -1 otherwise
 */
int get_device_bdaddr(int device_number, char bdaddr[18])
{
    bdaddr_t bda;

    int s = hci_open_dev (device_number);

    if(hci_read_bd_addr(s, &bda, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    ba2str(&bda, bdaddr);
    return 0;
}

/*
 * \brief This function writes the bluetooth device class for a given device number.
 *
 * \param device_number  the device number
 *
 * \return 0 if successful, -1 otherwise
 */
int write_device_class(int device_number, uint32_t class)
{
    int s = hci_open_dev (device_number);

    if(hci_write_class_of_dev(s, class, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    return 0;
}

int delete_stored_link_key(int device_number, char* bdaddr)
{
    int s = hci_open_dev (device_number);

    bdaddr_t bda;
    str2ba(bdaddr, &bda);

    if(hci_delete_stored_link_key(s, &bda, 0, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    return 0;
}

int write_stored_link_key(int device_number, char* bdaddr, unsigned char* key)
{
    int s = hci_open_dev (device_number);

    bdaddr_t bda;
    str2ba(bdaddr, &bda);

    if(hci_write_stored_link_key(s, &bda, key, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    return 0;
}

int authenticate_link(int device_number, unsigned short handle)
{
    int s = hci_open_dev (device_number);

    if(hci_authenticate_link(s, handle, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    return 0;
}

int encrypt_link(int device_number, unsigned short handle)
{
    int s = hci_open_dev (device_number);

    if(hci_encrypt_link(s, handle, 0x01, HCI_REQ_TIMEOUT) < 0)
    {
        return -1;
    }

    return 0;
}
