#include "wvtest.h"
#include "uniconfroot.h"
#include "unitempgen.h"
#include "uniwatch.h"

WVTEST_MAIN("even more basic")
{
    UniTempGen g;
    WVFAIL(g.haschildren("/"));
    
    g.set("/", "blah");
    WVFAIL(g.haschildren("/"));
    
    g.set("/x", "pah");
    WVPASS(g.haschildren("/"));
    WVFAIL(g.haschildren("/x"));
    
    g.set("/", WvString());
    WVFAIL(g.haschildren("/"));
}


WVTEST_MAIN("basics")
{
    UniConfRoot cfg("temp:");
    WVFAIL(cfg.haschildren());
    
    cfg.setme("blah");
    WVFAIL(cfg.haschildren());
    
    cfg["x"].setme("pah");
    WVPASS(cfg.haschildren());
    WVFAIL(cfg["x"].haschildren());
    
    cfg.remove();
    WVFAIL(cfg.haschildren());
}

WVTEST_MAIN("trailing slashes")
{
    UniTempGen g;
    g.set("", "xyzzy");
    WVPASSEQ(g.get("/"), "xyzzy");
    WVPASSEQ(g.get("///"), "xyzzy");
    WVPASSEQ(g.get(""), "xyzzy");
    WVPASSEQ(g.get("//"), "xyzzy");

    WVPASSEQ(g.get("///simon"), WvString::null);
    WVPASSEQ(g.get("///simon/"), WvString::null);
    WVPASSEQ(g.get("/simon///"), WvString::null);

    g.set("//simon", "law");
    WVPASSEQ(g.get("/simon"), "law");
    WVPASSEQ(g.get("simon///"), WvString::null);

    g.set("//simon/", "LAW");
    WVPASSEQ(g.get("/simon"), "law");
    WVPASSEQ(g.get("simon///"), WvString::null);
    WVFAIL(g.haschildren("simon//"));

    g.set("//simon/law", "1");
    WVPASSEQ(g.get("/simon/law"), "1");
    g.set("//simon/", WvString::null);
    WVPASSEQ(g.get("/simon/law"), WvString::null);
    WVFAIL(g.haschildren("simon///"));

    UniConfRoot cfg("temp:");
    cfg.setme("xyzzy");
    WVPASSEQ(cfg.xget("/"), "xyzzy");
    WVPASSEQ(cfg.xget("///"), "xyzzy");
    WVPASSEQ(cfg[""].getme(), "xyzzy");
    WVPASSEQ(cfg[""][""]["/"].xget("/"), "xyzzy");

    WVPASSEQ(cfg[""][""]["/"]["simon"].xget(""), WvString::null);
    WVPASSEQ(cfg[""][""]["/"]["simon"].xget("///"), WvString::null);

    cfg[""]["/"][""]["simon"].setme("law");
    WVPASSEQ(cfg[""][""]["/"].xget("simon"), "law");
    WVPASSEQ(cfg[""][""]["/"]["simon"].getme(), "law");
    WVPASSEQ(cfg[""][""]["/"]["simon"].xget(""), WvString::null);
    WVPASSEQ(cfg[""][""]["/"]["simon"].xget("///"), WvString::null);

    cfg[""]["/"][""]["simon"].xset("/", "LAW");
    WVPASSEQ(cfg[""][""]["/"].xget("simon"), "law");
    WVPASSEQ(cfg[""][""]["/"].xget("simon/"), "");
    WVPASSEQ(cfg[""][""]["/"]["simon"].getme(), "law");
    WVPASSEQ(cfg[""][""]["/"]["simon"].xget(""), WvString::null);
    WVPASSEQ(cfg[""][""]["/"]["simon"].xget("///"), WvString::null);
    WVFAIL(cfg[""]["simon"][""].haschildren());
    WVFAIL(cfg["simon"]["/"].haschildren());

    cfg[""][""]["/"]["simon"].xset("/law", "1");
    WVPASSEQ(cfg[""][""]["/"]["simon"][""].xget("/law"), "1");
    cfg[""][""]["/"]["simon"]["law"].xset("", "2");
    WVPASSEQ(cfg[""][""]["/"]["simon"][""].xget("/law"), "1");
    cfg[""][""]["/"]["simon"]["law"].xset("/", "3");
    WVPASSEQ(cfg[""][""]["/"]["simon"][""].xget("/law"), "1");
    cfg[""]["/"][""]["simon"].xset("/", "LAW");
    WVPASSEQ(cfg[""][""]["/"].xget("simon"), "law");
    WVPASSEQ(cfg[""][""]["/"].xget("simon/"), "");
    cfg[""][""]["/"]["simon"][""].remove();
    WVPASSEQ(cfg[""][""]["/"]["simon"][""].xget("/law"), WvString::null);
    WVFAIL(cfg[""]["simon"]["/"].haschildren());

    WVPASSEQ(cfg[""].getme(), "xyzzy");
}

static int count;
static void callback(const UniConf keyconf, const UniConfKey key)
{
    printf("callback: '%s', '%s'\n",
	   keyconf[key].fullkey().cstr(), keyconf.xget(key).cstr());

    ++count;
}

WVTEST_MAIN("UniTempGen callbacks")
{
    UniConfRoot root(new UniTempGen());
    UniConf uni(root["tmp/idbd"]);
    int prev = count = 0;

    WVPASSEQ(count, prev);
    prev = count;
    UniWatch watch(uni["cmd"], &callback);
    WVPASSEQ(count, prev);
    prev = count;

    uni["cmd"].remove();
    WVPASSEQ(count, prev);
    prev = count;

    uni["cmd"].remove();
    WVPASSEQ(count, prev);
    prev = count;

    uni["cmd"].xset("tmp", "foo");
    WVPASSEQ(count, prev += 2);
    prev = count;

    uni["cmd"]["bar"].remove();
    WVPASSEQ(count, prev);
    prev = count;
    uni["cmd"].xset("bar", "baz");
    WVPASSEQ(count, prev += 1);
    prev = count;

    uni["cmd"].remove();
    WVPASSEQ(count, prev += 3);
    prev = count;
}

// FIXME: could test lots more stuff here...
