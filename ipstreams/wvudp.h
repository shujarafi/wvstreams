/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1998 Worldvisions Computer Technology, Inc.
 * 
 * WvUDPStream can send and receive packets on a connectionless UDP socket.
 * 
 * In the constructor, the socket is attached using bind() to the given
 * _local address.  If the address is 0.0.0.0, all addresses on the local
 * host are used; if the port is 0, an available port number is chosen
 * automatically.
 * 
 * If the _rem address is 0.0.0.0, the port is not connect()ed.  That means
 * it can receive packets from anywhere and send them to anywhere.  The
 * src() and setdest() functions are useful for this.  If _rem is not 0.0.0.0,
 * connect() is called and the socket will only accept data to/from the
 * specified remote UDP address.
 * 
 * Buffering: all the usual WvStream-style input buffering is available,
 * including getline(), but because input packets may get lost it is of
 * limited usefulness.  Buffering will cause particular confusion if the
 * socket is not connect()ed.
 */
#ifndef __WVUDP_H
#define __WVUDP_H

#include "wvstream.h"
#include "wvaddr.h"


class WvUDPStream : public WvStream
{
public:
    // connect a new socket
    WvUDPStream(const WvIPPortAddr &_local, const WvIPPortAddr &_rem);
    virtual ~WvUDPStream();
    
    const WvAddr *local() const;
    
    // return the remote address (source of incoming packets, target of
    // outgoing packets).  This is the last host sent to or received from,
    // whichever was more recent.
    virtual const WvAddr *src() const;
    void setdest(const WvIPPortAddr &_remaddr)
        { remaddr = _remaddr; }

protected:
    WvIPPortAddr localaddr, remaddr;
    
    virtual size_t uread(void *buf, size_t count);
    virtual size_t uwrite(const void *buf, size_t count);
};


#endif // __WVUDP_H
