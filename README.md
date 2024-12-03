# monitor

Ages ago, our home Internet connection was a DSL link that required the client side to establish a connection with PPP, instead of just persisently routing IP packets. At the time, FreeBSD had some sort of kernel bug, and if `pppd` were left running for a long time, after a while, it'd start failing to transmit packets with the error "No buffer space available". When this happened, stopping `pppd` would release whatever resources had leaked and it could be restarted. I ended up writing this utility to start `pppd` and then monitor a `ping` process for "No buffer space available". When the error happens, the `ping` and `pppd` are both killed, and then the whole process loops.

I haven't needed this in, I dunno, over 20 years now. :-P Preserving the code for posterity.
