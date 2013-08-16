portserver is used to get multiple access to one physical serial port device.

                    pty1 --------------
application 1   ---------|            |
                         |            |
                    ptyN | portserver |------ /dev/ttySX (serial port)
application N   ---------|            |
                         |            |
                         --------------

Operation:
- all data received on any pty device is written to the serial port device
- data received from the serial port device is written to all pty devices

Note: to get consistent, frame based data transmission the write access to 
      pty1..N needs to be also frame based, i.e. write one data frame with
      write access. Fragmented frame writes will result in randomly merged 
      data transmitted by the serial device

portserver is started with the serial port device name as parameter. portserver
creates a control pty named /tmp/busportserver/_dev_ttySX_cmdPts. The name 
of the serial port device is mapped to the name of control pty by replacig the 
'/' by '_' and by appending '_cmdPts'. The control pty allows two command: 
create a new pty device and remove an existing pty device (see 
portservergetdev.sh for details when creating a new pty).

Due to usage of pty devices the portserver devices (pty1..N and also /dev/ttySX)
can be connected with socat and ser2net:

                   --------------
ser2net1 ----------|            |
                   |            |
ser2net2 ----------| portserver |--- socat device -----------
                   |            |                           |
local application -|            |                           | 
                   --------------                           |
                                                            |      
 -----------------------------TCP/UDP------------------------
 |
 |                 --------------
 ---- ser2net -----|            |         
                   | portserver |--- real serial device
local application--|            |
                   --------------
