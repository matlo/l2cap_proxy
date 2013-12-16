/*
 Copyright (c) 2010 Mathieu Laurendeau <mat.lau@laposte.net>
   License: GPLv3
*/

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

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

    if(hci_read_bd_addr(s, &bda, 1000) < 0)
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

    if(hci_write_class_of_dev(s, class, 1000) < 0)
    {
        return -1;
    }

    return 0;
}
