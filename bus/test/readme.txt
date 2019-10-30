testing environment:

        ptyA            ptyB
bustest------ forwarder------ ttyechoserver

Use forwarder and ttyechoserver from tool/portserver/test

starting a test run:
(1) run forwarder. It prints the names of 2 pty devices (ptyA and ptyB)
(2) run ttyechoserver with ptyB as parameter
(3) run bustest with ptyA as parameter
