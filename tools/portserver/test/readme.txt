testing environment:

                    pty1 --------------
ttyechoclient 1 ---------|            |
                         |            | ptyA            ptyB
                    ptyN | portserver |------ forwarder------ ttyechoserver
ttyechoclient N ---------|            |
                         |            |
                         --------------

starting a test run:
(1) run forwarder. It prints the names of 2 pty devices (ptyA and ptyB)
(2) run ttyechoserver with ptyB as parameter
(3) run portserver with ptyA as parameter
(4) use portservergetdev.sh with ptyA to create a new pty dev by portserver
(5) run ttyechoclient with the name returned by portservergetdev.sh and a char
    string as parameter
(6) repeat (4) and (5) to get multiple ttyechoclients
