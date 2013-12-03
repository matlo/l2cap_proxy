l2cap_proxy
===========

This tool is a l2cap proxy for HID devices.  
It only runs on Linux, and it requires a bluetooth dongle.  
The master and the device have to be paired with the bluetooth dongle.  
It was only tested with a PS3 and a DS3.  

Instructions for Ubuntu-based distros:  

sudo apt-get install libbluetooth-dev  
sudo service bluetooth stop  
sudo hciconfig hci0 up pscan  
sudo ./l2cap <master bdaddr>  

In debian the bluetooth service is automatically started when a device tries to connect.  
To disable the service, run the following command and reboot:  
sudo update-rc.d triggerhappy disable  
