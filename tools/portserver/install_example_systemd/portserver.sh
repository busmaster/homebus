#!/bin/sh

# /dev/hausbus0    tcp:2000 unused (formerly rpicam1)
# /dev/hausbus1    tcp:2001 unused (formerly ap420)
# /dev/hausbus2    tcp:2002 unused (formerly zbox)
# /dev/hausbus3    tcp:2003 unused (formerly rpicam2)
# /dev/hausbus4    tcp:2004 test
# /dev/hausbus5    tcp:2005 nuc
#
# /dev/hausbus10   local test, monitor
# /dev/hausbus11   cron
# /dev/hausbus12   mqtt
# /dev/hausbus13   unused
# /dev/hausbus14   unused
#

OPTS=/dev/ttyACM0

start-stop-daemon -b --start --exec /usr/bin/portserver -- $OPTS
sleep 2

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus0
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus1
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus2
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus3
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus4
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus5
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus10
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus11
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus12
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus13
chgrp dialout $PTS
chmod g+r $PTS

PTS=$(/usr/bin/portservergetdev.sh $OPTS)
ln -s $PTS /dev/hausbus14
chgrp dialout $PTS
chmod g+r $PTS
