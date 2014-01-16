l2cap_proxy
===========

This tool is a l2cap proxy.  
It performs like this:  

bt device <----> PC <----> bt master  

It was tested with:  
* a PS3 and a DS3 (runs over the HID control + HID interrupt PSMs)
* a PS4 and a DS4 (runs over the SDP + HID control + HID interrupt PSMs)

It should work without modification for all the predefined l2cap PSMs (https://www.bluetooth.org/en-us/specification/assigned-numbers/logical-link-control).  
Other PSMs can be easily added in the source code.  
It only runs on Linux (tested with Ubuntu and Debian), and it requires at least one bluetooth dongle (two may be useful for high packet rates).  
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

Bluetooth authentication
------------------------

For bluetooth devices that use authentication like the PS4/DS4, a link key has to be provided to the linux kernel.  
This can be done with the following commands:
```
sudo service bluetooth stop  
sudo echo "<bdaddr> <link key> 4 0" >> /var/lib/bluetooth/<dongle bdaddr>/linkkeys
sudo service bluetooth start  
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
