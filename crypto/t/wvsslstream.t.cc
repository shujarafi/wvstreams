#include "wvtest.h"
#include "wvsslstream.h"
#include "wvloopback2.h"
#include "wvx509.h"
#include "wvistreamlist.h"
#include <signal.h>

static void run(WvStream &list, WvStream *s1, WvStream *s2)
{
    for (int i = 0; i < 100; i++)
    {
	list.runonce(10);
	if ((s1 && s1->isreadable()) || (s2 && s2->isreadable()))
	    break;
    }
}


static void sslloop(WvIStreamList &list, WvX509Mgr &x509,
		    WvSSLStream *&s1, WvSSLStream *&s2)
{
    signal(SIGPIPE, SIG_IGN);
    IWvStream *_s1, *_s2;
    wvloopback2(_s1, _s2);
    
    s1 = new WvSSLStream(_s1, &x509, 0, true);
    s2 = new WvSSLStream(_s2);
    
    list.auto_prune = false;
    list.append(s1, true, "s1");
    list.append(s2, true, "s2");
}


WVTEST_MAIN("crypto basics")
{
    WvX509Mgr x509("cn=random_stupid_dn", 1024);
    WvIStreamList list;
    WvSSLStream *s1, *s2;
    sslloop(list, x509, s1, s2);
 
    // test some basic data transfer
    WVPASS(s1->isok());
    WVPASS(s2->isok());
    s1->print("s1 output\ns1 line2\n");
    s2->print("s2 output\ns2 line2\n");
    run(list, s1, s2);
    WVPASSEQ(s1->blocking_getline(1000), "s2 output");
    WVPASSEQ(s2->blocking_getline(1000), "s1 output");
    WVPASSEQ(s2->getline(), "s1 line2");
    WVPASSEQ(s1->blocking_getline(1000), "s2 line2");
    
    // test special close() behaviour.  At one point, WvSSLStream::uread()
    // would return nonzero, but close the stream anyway *before* returning.
    // This causes very confusing behaviour, because the calling stream
    // receives the SSL stream's closecallback() before getting the final
    // data from uread().  Thus, uread() should only call close() if it's
    // planning to return 0.
    s2->nowrite();
    s1->print("foostring");
    run(list, s1, s2);
    s1->cloned->close();
    list.unlink(s1); // auto_prune disabled, so we need to do this by hand
    run(list, NULL, s2);
    
    WvDynBuf buf;
    WVPASSEQ(s2->read(buf, 1024), 9);
    WVPASSEQ(buf.getstr(), "foostring");
    WVPASS(s2->isok());
    run(list, NULL, s2);
    WVPASS(s2->isok());
    
    WVPASSEQ(s2->read(buf, 1024), 0);
    WVFAIL(s2->isreadable());
    run(list, NULL, s2);
    WVPASSEQ(s2->read(buf, 1024), 0);
    WVFAIL(s2->isok());
}


WVTEST_MAIN("sslconnect")
{
    WvX509Mgr x509("cn=random_stupid_dn", 1024);
    WvIStreamList list;
    WvSSLStream *s1, *s2;
    sslloop(list, x509, s1, s2);
    
    s1->print("hello\n");
    WvDynBuf buf;
    WVPASSEQ(s1->read(buf, 1024), 0);
 
    // make sure s1 has sent/received all the data currently in its pipe,
    // but without giving s2 a chance to run
    for (int i = 0; i < 50; i++)
	s1->runonce(5);
    
    WVFAIL(s1->isreadable());
    
    for (int i = 0; i < 50; i++)
	s2->runonce(5);
    
    WVFAIL(s2->isreadable());
}


WVTEST_MAIN("sslclose 1")
{
    WvX509Mgr x509("cn=random_stupid_dn", 1024);
    WvIStreamList list;
    WvSSLStream *s1, *s2;
    sslloop(list, x509, s1, s2);
 
    s2->print("hello\n");
    run(list, s1, s2);
    WVPASSEQ(s1->getline(), "hello");
    s2->cloned->close();
    run(list, s1, s2);
    list.unlink(s2);
    run(list, s1, NULL);
    WvDynBuf buf;
    WVPASSEQ(s1->read(buf, 1024), 0);
    WVFAIL(s1->isok());
}


// like sslclose1, but shutting down SSL politely (ie. closing the sslstream
// instead of the underlying stream).  This tests a different code path
// in WvSSLStream.
WVTEST_MAIN("sslclose 2")
{
    WvX509Mgr x509("cn=random_stupid_dn", 1024);
    WvIStreamList list;
    WvSSLStream *s1, *s2;
    sslloop(list, x509, s1, s2);
 
    s2->print("hello\n");
    run(list, s1, s2);
    WVPASSEQ(s1->getline(), "hello");
    s2->close();
    run(list, s1, s2);
    list.unlink(s2);
    run(list, s1, NULL);
    WvDynBuf buf;
    WVPASSEQ(s1->read(buf, 1024), 0);
    WVFAIL(s1->isok());
}


// This test relies on the fact that reading a stream will never cause it
// to go !isok() until the inbuf is actually empty.  I'm not sure if that's
// desirable behaviour or not, but it definitely isn't the *current*
// behaviour.  Rather than try to fix it, let's comment out the test for now;
// there seems to be no remaining way to actually aggravate the problem other
// than what we do in this test (which is definitely impolite behaviour).
//    -- apenwarr (2004/11/09)
#if INBUF_CONTENT_ALWAYS_PREVENTS_NOT_ISOK
WVTEST_MAIN("ssl inbuf after read error")
{
    WvX509Mgr x509("cn=random_stupid_dn", 1024);
    WvIStreamList list;
    WvSSLStream *s1, *s2;
    sslloop(list, x509, s1, s2);
    
    // connect up
    run(list, s1, s2);
    
    s2->nowrite();
    s1->print("1\n2\n");
    s1->flush(0);
    WVPASS(1);
    run(list, s1, s2);
    s1->print("3\n4");
    WVPASS(1);
    run(list, s1, s2);
    s1->cloned->close();
    WVPASS(1);
    run(list, s1, s2);
    WVFAIL(s1->isok());
    
    WVPASSEQ(s2->blocking_getline(1000), "1");
    WVPASSEQ(s2->blocking_getline(1000), "2");
    WVPASS(s2->isok());
    
    // force SSL to try reading again, even though it doesn't need to.  This
    // emulates a poorly-behaved wrapper stream, like maybe WvEncoderStream.
    s2->queuemin(1024);
    char buf[1024];
    WVFAIL(s2->read(buf, 1024));
    WVFAIL(s2->read(buf, 1024));
    s2->queuemin(0);
    
    // inbuf still has data!
    WVPASS(s2->isok());
    
    WVPASSEQ(s2->blocking_getline(1000), "3");
    WVPASSEQ(s2->blocking_getline(1000), "4");
    WVFAIL(s2->blocking_getline(1000));
    WVFAIL(s2->isok());
}
#endif