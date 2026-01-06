This example configuration will set up 10 pty devices to get access to
one serial port device. The serial port device is selected by OPTS in portserver
init script. In portserver init script symbolic link files /dev/hausbus0..4 
and /dev/hausbus/10..14 are associated to the pty devices. 
/dev/hausbus0..4 are mapped to TCP and /dev/hausbus10..14 are intended to be 
used locally.
ser2net maps /dev/hausbus0..4 to TCP ports 2000 .. 2004 (see ser2net.conf)

(1) copy portserver.sh to /root/portserver.sh and set serial port to use into OPTS
(2) copy portserver.service to /etc/systemd/system/portserver.service
(3) copy ser2net.conf to /etc/ser2net.conf
(4) copy portservergetdev.sh to /usr/bin
(5) copy bin/portserver to /usr/bin
(5) systemctl enable portserver.service
(6) restart ser2net
