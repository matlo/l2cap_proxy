l2cap_proxy
===========

This tool is a l2cap proxy for HID devices.  
It performs like this:  

bt device <----> PC <----> bt master  

It was only tested with a PS3 and a DS3.  
It only runs on Linux (tested with Ubuntu and Debian), and it requires a bluetooth dongle.  
The master and the device have to be paired with the bluetooth dongle.  

Compilation
-----------
```
sudo apt-get install libbluetooth-dev  
sudo apt-get install git  
git clone https://github.com/matlo/l2cap_proxy.git  
cd l2cap_proxy  
make  
```

Run the proxy
-------------
```
sudo service bluetooth stop  
sudo hciconfig hci0 up pscan  
sudo ./l2cap master_bdaddr  
```

In Debian the bluetooth service is automatically started when a device tries to connect.
This is annoying since it will intercept the connection requests.    
To disable the service, run the following command and reboot:  
```
sudo update-rc.d triggerhappy disable  
```

Capture the bluetooth traffic
-----------------------------
```
sudo apt-get install bluez-hcidump  
sudo hcidump -l 4096 -w capture.dump  
```

Power on the bluetooth device.  
To stop the capture (hcidump), press ctrl+c.  
  
capture.dump can be opened with wireshark.  
