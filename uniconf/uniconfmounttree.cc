/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * Defines a UniConfGen that manages a tree of UniConfGen instances.
 */
#include "unimounttreegen.h"
#include "wvmoniker.h"

/***** UniMountTreeGen *****/

UniMountTreeGen::UniMountTreeGen()
{
    mounts = new UniMountTree(NULL, UniConfKey::EMPTY);
}


UniMountTreeGen::~UniMountTreeGen()
{
    // destroys all generators
    delete mounts;
}


WvString UniMountTreeGen::get(const UniConfKey &key)
{
    // consult the generators
    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        WvString result = gen->get(it.tail());
        if (!result.isnull())
            return result;
    }
    
    // ensure key exists if it is in the path of a mountpoint
    UniMountTree *node = mounts->find(key);
    if (node)
        return ""; // fake read-only key not provided by anyone

    // no matches
    return WvString::null;
}


bool UniMountTreeGen::set(const UniConfKey &key, WvStringParm value)
{
    // update the generator that defines the key, if any
    UniConfKey mountpoint;
    UniConfGen *provider = whichmount(key, &mountpoint);
    if (provider)
        return provider->set(mountpoint, value);
    return false;
}


bool UniMountTreeGen::zap(const UniConfKey &key)
{
    bool success = true;
    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        if (!gen->zap(it.tail()))
            success = false;
    }
    // FIXME: need to recurse over generators of descendent keys
    //        to zap all of their contents also
    return success;
}


bool UniMountTreeGen::exists(const UniConfKey &key)
{
    // ensure key exists if it is in the path of a mountpoint
    UniMountTree *node = mounts->find(key);
    if (node)
        return true;
    
    // otherwise consult the generators
    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        if (gen->exists(it.tail()))
            return true;
    }

    // no match
    return false;
}


bool UniMountTreeGen::haschildren(const UniConfKey &key)
{
    UniMountTree *node = mounts->find(key);
    if (node && node->haschildren())
        return true;

    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        if (gen->haschildren(it.tail()))
            return true;
    }
    return false;
}


bool UniMountTreeGen::refresh(const UniConfKey &key, UniConfDepth::Type depth)
{
    return dorecursive(genrefreshfunc, key, depth);
}


bool UniMountTreeGen::genrefreshfunc(UniConfGen *gen,
    const UniConfKey &key, UniConfDepth::Type depth)
{
    return gen->refresh(key, depth);
}


bool UniMountTreeGen::commit(const UniConfKey &key, UniConfDepth::Type depth)
{
    return dorecursive(gencommitfunc, key, depth);
}


bool UniMountTreeGen::gencommitfunc(UniConfGen *gen,
    const UniConfKey &key, UniConfDepth::Type depth)
{
    return gen->commit(key, depth);
}


bool UniMountTreeGen::dorecursive(GenFunc func, const UniConfKey &key,
    UniConfDepth::Type depth)
{
    // do containing generators
    bool success = true;
    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        if (!func(gen, it.tail(), depth))
            success = false;
    }

    // do recursive
    if (depth != UniConfDepth::ZERO)
    {
        UniMountTree *node = mounts->find(key);
        if (node && ! dorecursivehelper(func, node, depth))
            success = false;
    }
    return success;
}


bool UniMountTreeGen::dorecursivehelper(GenFunc func,
    UniMountTree *node, UniConfDepth::Type depth)
{
    // determine depth for next step
    switch (depth)
    {
        case UniConfDepth::ZERO:
            assert(false);
            return true;

        case UniConfDepth::ONE:
        case UniConfDepth::CHILDREN:
            depth = UniConfDepth::ZERO;
            break;

        case UniConfDepth::DESCENDENTS:
            depth = UniConfDepth::INFINITE;
        case UniConfDepth::INFINITE:
            break;
    }

    // process nodes and recurse if needed
    bool success = true;
    UniMountTree::Iter it(*node);
    for (it.rewind(); it.next(); )
    {
        UniConfGenList::Iter genit(it->generators);
        for (genit.rewind(); genit.next(); )
        {
            if (! func(genit.ptr(), UniConfKey::EMPTY, depth))
                success = false;
        }
        if (depth != UniConfDepth::ZERO)
        {
            if (! dorecursivehelper(func, it.ptr(), depth))
                success = false;
        }
    }
    return success;
}
 
 
UniConfGen *UniMountTreeGen::mount(const UniConfKey &key,
    WvStringParm moniker, bool refresh)
{
    UniConfGen *gen = wvcreate<UniConfGen>(moniker);
    if (gen)
        mountgen(key, gen, refresh); // assume always succeeds for now
    return gen;
}


UniConfGen *UniMountTreeGen::mountgen(const UniConfKey &key,
    UniConfGen *gen, bool refresh)
{
    UniMountTree *node = mounts->findormake(key);
    node->generators.append(gen, true);
    gen->setcallback(UniConfGenCallback(this,
        &UniMountTreeGen::gencallback), node);
    if (gen && refresh)
        gen->refresh(UniConfKey::EMPTY, UniConfDepth::INFINITE);
    return gen;
}


void UniMountTreeGen::unmount(const UniConfKey &key,
    UniConfGen *gen, bool commit)
{
    UniMountTree *node = mounts->find(key);
    if (!node)
        return;

    UniConfGenList::Iter genit(node->generators);
    if (! genit.find(gen))
        return;

    if (commit)
        gen->commit(UniConfKey::EMPTY, UniConfDepth::INFINITE);
    gen->setcallback(UniConfGenCallback(), NULL);

    node->generators.unlink(gen);
}


UniConfGen *UniMountTreeGen::whichmount(const UniConfKey &key,
    UniConfKey *mountpoint)
{
    // see if a generator acknowledges the key
    UniMountTree::GenIter it(*mounts, key);
    for (it.rewind(); it.next(); )
    {
        UniConfGen *gen = it.ptr();
        if (gen->exists(it.tail()))
            goto found;
    }
    // find the generator that would be used to set the value
    it.rewind();
    if (! it.next())
        return NULL;

found:
    if (mountpoint)
        *mountpoint = it.tail();
    return it.ptr();
}


bool UniMountTreeGen::ismountpoint(const UniConfKey &key)
{
    UniMountTree *node = mounts->find(key);
    return node && ! node->generators.isempty();
}


UniMountTreeGen::Iter *UniMountTreeGen::iterator(const UniConfKey &key)
{
    return new Iter(*this, key);
}


void UniMountTreeGen::prune(UniMountTree *node)
{
    while (node != mounts && !node->isessential())
    {
        UniMountTree *next = node->parent();
        delete node;
        node = next;
    }
}


void UniMountTreeGen::gencallback(const UniConfGen &gen,
    const UniConfKey &key, UniConfDepth::Type depth, void *userdata)
{
    UniMountTree *node = static_cast<UniMountTree*>(userdata);
    delta(UniConfKey(node->fullkey(), key), depth);
}


/***** UniMountTreeGen::Iter *****/

UniMountTreeGen::Iter::Iter(UniMountTreeGen &root, const UniConfKey &key) 
    : xroot(&root), xkey(key), genit(*root.mounts, key),
        hack(71), hackit(hack)
{
}


void UniMountTreeGen::Iter::rewind()
{
    hack.zap();

    // find nodes provided by the root of any mount points.
    // (if we want to iterate over /foo, then mounts directly on /foo/blah and
    // /foo/snoo must be included)
    UniMountTree *node = xroot->mounts->find(xkey);
    if (node)
    {
        UniMountTree::Iter nodeit(*node);
        for (nodeit.rewind(); nodeit.next(); )
            hack.add(new WvString(nodeit->key()), true);
    }

    // walk through *all* generators and add any appropriate sub-keys
    // provided by each generator.
    for (genit.rewind(); genit.next(); )
    {
        UniConfGen *gen = genit.ptr();
        UniConfAbstractIter *keyit = gen->iterator(genit.tail());
        for (keyit->rewind(); keyit->next(); )
            hack.add(new WvString(keyit->key()), true);
        delete keyit;
    }

    hackit.rewind();
}


bool UniMountTreeGen::Iter::next()
{
    return hackit.next();
}


UniConfKey UniMountTreeGen::Iter::key() const
{
    return UniConfKey(hackit());
}



/***** UniMountTree *****/

UniMountTree::UniMountTree(UniMountTree *parent,
    const UniConfKey &key) :
    UniConfTree<UniMountTree>(parent, key)
{
}


UniMountTree::~UniMountTree()
{
}


UniMountTree *UniMountTree::findnearest(const UniConfKey &key,
    int &split)
{
    split = 0;
    UniMountTree *node = this;
    UniConfKey::Iter it(key);
    for (it.rewind(); it.next(); split++)
    {
        UniMountTree *next = node->findchild(it());
        if (!next)
            break;
        node = next;
    }
    return node;
}


UniMountTree *UniMountTree::findormake(const UniConfKey &key)
{
    UniMountTree *node = this;
    UniConfKey::Iter it(key);
    for (it.rewind(); it.next(); )
    {
        UniMountTree *prev = node;
        node = prev->findchild(it());
        if (!node)
            node = new UniMountTree(prev, it());
    }
    return node;
}



/***** UniMountTree::MountIter *****/

UniMountTree::MountIter::MountIter(UniMountTree &root,
    const UniConfKey &key)
    : xkey(key)
{
    bestnode = root.findnearest(key, bestsplit);
}


void UniMountTree::MountIter::rewind()
{
    xnode = NULL;
}


bool UniMountTree::MountIter::next()
{
    if (! xnode)
    {
        xsplit = bestsplit;
        xnode = bestnode;
    }
    else if (xsplit != 0)
    {
        xsplit -= 1;
        xnode = xnode->parent();
    }
    else
        return false;
    return true;
}



/***** UniMountTree::GenIter *****/

UniMountTree::GenIter::GenIter(UniMountTree &root,
    const UniConfKey &key) :
    UniMountTree::MountIter(root, key),
    genit(NULL)
{
}


UniMountTree::GenIter::~GenIter()
{
    delete genit;
}


void UniMountTree::GenIter::rewind()
{
    if (genit)
    {
        delete genit;
        genit = NULL;
    }
    UniMountTree::MountIter::rewind();
}


bool UniMountTree::GenIter::next()
{
    for (;;)
    {
        if (genit && genit->next())
            return true;
        if (! UniMountTree::MountIter::next())
            return false;

        genit = new UniConfGenList::Iter(node()->generators);
        genit->rewind();
    }
    return false;
}
