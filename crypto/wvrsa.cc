/*
 * Worldvisions Tunnel Vision Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * RSA cryptography abstractions.
 */
#include "wvsslhacks.h"
#include "wvrsa.h"
#include "wvhex.h"
#include <assert.h>
#include <rsa.h>

/***** WvRSAKey *****/

WvRSAKey::WvRSAKey(const WvRSAKey &k)
{
    if (k.prv)
	init(k.private_str(), true);
    else
	init(k.public_str(), false);
}


WvRSAKey::WvRSAKey(struct rsa_st *_rsa, bool priv)
{
    rsa = _rsa;
    pub = hexifypub(rsa);
    if (priv)
        prv = hexifyprv(rsa);
}


WvRSAKey::WvRSAKey(WvStringParm keystr, bool priv)
{
    init(keystr, priv);
}


WvRSAKey::WvRSAKey(int bits)
{
    rsa = RSA_generate_key(bits, 0x10001, NULL, NULL);
    pub = hexifypub(rsa);
    prv = hexifyprv(rsa);
}


WvRSAKey::~WvRSAKey()
{
    if (rsa)
        RSA_free(rsa);
}


void WvRSAKey::init(WvStringParm keystr, bool priv)
{
    rsa = NULL;
    
    // unhexify the supplied key
    WvBuffer keybuf;
    if (! WvHexDecoder().flush(keystr, keybuf) || keybuf.used() == 0)
    {
        seterr("RSA key is not a valid hex string");
        return;
    }
    size_t keylen = keybuf.used();
    const unsigned char *key = keybuf.get(keylen);
    
    // create the RSA struct
    if (priv)
    {
	rsa = wv_d2i_RSAPrivateKey(NULL, & key, keylen);
        if (rsa != NULL)
        {
            prv = keystr;
            pub = hexifypub(rsa);
        }
    }
    else
    {
	rsa = wv_d2i_RSAPublicKey(NULL, & key, keylen);
        if (rsa != NULL)
        {
            prv = WvString::null;
            pub = keystr;
        }
    }
    if (rsa == NULL)
        seterr("RSA key is invalid");
}


#if 0
void WvRSAKey::pem2hex(WvStringParm filename)
{
    RSA *rsa = NULL;
    FILE *fp;

    fp = fopen(filename, "r");

    if (!fp)
    {
	seterr("Unable to open %s!",filename);
	return;
    }

    rsa = PEM_read_RSAPrivateKey(fp,NULL,NULL,NULL);

    fclose(fp);

    if (!rsa)
    {
	seterr("Unable to decode PEM File!");
	return;
    }
    else
    {
	hexify(rsa);
	return;
    }
}
#endif


WvString WvRSAKey::hexifypub(RSA *rsa)
{
    WvBuffer keybuf;
    size_t size = i2d_RSAPublicKey(rsa, NULL);
    unsigned char *key = keybuf.alloc(size);
    size_t newsize = i2d_RSAPublicKey(rsa, & key);
    assert(size == newsize);
    
    WvString keystr = WvHexEncoder().strflush(keybuf);
    return keystr;
}


WvString WvRSAKey::hexifyprv(RSA *rsa)
{
    WvBuffer keybuf;
    size_t size = i2d_RSAPrivateKey(rsa, NULL);
    unsigned char *key = keybuf.alloc(size);
    size_t newsize = i2d_RSAPrivateKey(rsa, & key);
    assert(size == newsize);
    
    WvString keystr = WvHexEncoder().strflush(keybuf);
    return keystr;
}


/***** WvRSAEncoder *****/

WvRSAEncoder::WvRSAEncoder(bool _encrypt, const WvRSAKey & _key) :
    encrypt(_encrypt), key(_key)
{
    if (key.isok() && key.rsa != NULL)
        rsasize = RSA_size(key.rsa);
    else
        rsasize = 0; // BAD KEY! (should assert but would break compatibility)
}


WvRSAEncoder::~WvRSAEncoder()
{
}


bool WvRSAEncoder::encode(WvBuffer &in, WvBuffer &out, bool flush)
{
    if (rsasize == 0)
    {
        // IGNORE BAD KEY!
        in.zap();
        return false;
    }
        
    bool success = true;
    if (encrypt)
    {
        // reserve space for PKCS1_PADDING
        const size_t maxchunklen = rsasize - 12;
        while (in.used() != 0)
        {
            size_t chunklen = in.used();
            if (chunklen >= maxchunklen)
                chunklen = maxchunklen;
            else if (! flush)
                break;

            // encrypt a chunk
            unsigned char *data = in.get(chunklen);
            unsigned char *crypt = out.alloc(rsasize);
            size_t cryptlen = RSA_public_encrypt(chunklen, data, crypt,
                key.rsa, RSA_PKCS1_PADDING);
            if (cryptlen != rsasize)
            {
                out.unalloc(rsasize);
                success = false;
            }
        }
    } else {
        const size_t chunklen = rsasize;
        while (in.used() >= chunklen)
        {
            // decrypt a chunk
            unsigned char *crypt = in.get(chunklen);
            unsigned char *data = out.alloc(rsasize);
            int cryptlen = RSA_private_decrypt(chunklen, crypt, data,
                key.rsa, RSA_PKCS1_PADDING);
            if (cryptlen == -1)
            {
                out.unalloc(rsasize);
                success = false;
            }
            else
                out.unalloc(rsasize - cryptlen);
        }
        // flush does not make sense for us here
        if (flush && in.used() != 0)
            success = false;
    }
    return success;
}


/***** WvRSAStream *****/

WvRSAStream::WvRSAStream(WvStream *_cloned,
    const WvRSAKey &_my_private_key, const WvRSAKey &_their_public_key) :
    WvEncoderStream(_cloned)
{
    readchain.append(new WvRSAEncoder(false /*encrypt*/,
        _my_private_key), true);
    writechain.append(new WvRSAEncoder(true /*encrypt*/,
        _their_public_key), true);
    if (_my_private_key.isok() && _my_private_key.rsa)
        min_readsize = RSA_size(_my_private_key.rsa);
}
