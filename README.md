l2cap_proxy
===========

This tool is a l2cap proxy.  
It performs like this:  

bt device <----> PC <----> bt master  

It was only tested with a PS3 and a DS3 (which implements the HID profile).  
It should work without modification for all the predefined l2cap PSMs (https://www.bluetooth.org/en-us/specification/assigned-numbers/logical-link-control).  
Other PSMs can be easily added in the source code.  
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
sudo hciconfig hci<X> up pscan  
sudo ./l2cap <master-bdaddr> <dongle-bdaddr> <device-class>  
```
```
<X>: the device number (type hciconfig to list the available adapters)  
<master-bdaddr>: the bdaddr of the l2cap listener (mandatory)  
<dongle-bdaddr>: the bdaddr of the adapter to use to connect to the l2cap listener (optional; the first adapter is used in case none is specified)  
<device-class>: the device class to be applied to the adapter (optional; requires <dongle-bdaddr>; the default one is 0x508)  
```

In Debian the bluetooth service is automatically started when a device tries to connect.  
This is annoying since it will intercept the connection requests.  
To disable the service, run the following command and reboot:  
```
sudo update-rc.d bluetooth disable  
```
Moving the bluetoothd binary will also prevent it from being started:  
```
sudo mv /usr/sbin/bluetoothd /usr/sbin/bluetoothd.bk  
```
This trick is usefull when using a Live CD or a Live USB.  

Capture the bluetooth traffic
-----------------------------
```
sudo apt-get install bluez-hcidump  
sudo hcidump -l 4096 -w capture.dump  
```

Power on the bluetooth device.  
To stop the capture (hcidump), press ctrl+c.  
  
capture.dump can be opened with wireshark.  
