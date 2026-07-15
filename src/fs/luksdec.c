/*

    File: luksdec.c

    Native in-process LUKS1/LUKS2 read-only decryption.

    Replaces cryptsetup + losetup + device-mapper with a pure userspace
    implementation:
      - LUKS1 binary header parsing
      - LUKS2 binary header + JSON metadata parsing
      - PBKDF2 (OpenSSL) and Argon2i/Argon2id (vendored, src/vendor/argon2)
        passphrase key derivation
      - AF-splitter merge (anti-forensic information splitter)
      - keyslot area decryption + master key digest verification
      - payload decryption exposed through a read-only disk_t wrapper, so
        partition detection and filesystem drivers work exactly as they
        would on a /dev/mapper device

    Supported ciphers (cipher "aes" only):
      - chain modes: XTS, CBC
      - IV modes:    plain, plain64, essiv:<hash>
    i.e. aes-xts-plain64 (modern default), aes-cbc-essiv:sha256 and
    aes-cbc-plain64 (legacy pre-2013 volumes), etc.

    Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <openssl/evp.h>

#include "types.h"
#include "common.h"
#include "log.h"
#include "hdaccess.h"
#include "luks_struct.h"
#include "luksdec.h"
#include "vendor/argon2/argon2.h"

#define LUKS1_SECTOR_SIZE 512
#define LUKS2_MAGIC_PRIMARY   "LUKS\xba\xbe"
#define LUKS_KEYMATERIAL_SECTORS 512
#define LUKSDEC_MAX_JSON (4 * 1024 * 1024)
#define LUKSDEC_MAX_KEYBYTES 128
#define LUKSDEC_MAX_DIGEST 128

/* ------------------------------------------------------------------ */
/* Low level disk read helper (handles arbitrary offset/length)        */
/* ------------------------------------------------------------------ */

static int base_read(disk_t *base, void *buf, uint64_t offset, size_t len)
{
  const unsigned int ss = base->sector_size > 0 ? base->sector_size : 512;
  uint64_t aligned_off = (offset / ss) * ss;
  size_t head = (size_t)(offset - aligned_off);
  size_t total = head + len;
  size_t rounded = ((total + ss - 1) / ss) * ss;
  unsigned char *tmp = (unsigned char *)malloc(rounded);
  if(tmp == NULL)
    return -1;
  {
    size_t done = 0;
    while(done < rounded)
    {
      unsigned int chunk = 0x100000; /* 1 MiB per pread */
      int r;
      if(rounded - done < chunk)
        chunk = (unsigned int)(rounded - done);
      r = base->pread(base, tmp + done, chunk, aligned_off + done);
      if(r != (int)chunk)
      {
        free(tmp);
        return -1;
      }
      done += chunk;
    }
  }
  memcpy(buf, tmp + head, len);
  free(tmp);
  return 0;
}

/* ------------------------------------------------------------------ */
/* base64 decode                                                       */
/* ------------------------------------------------------------------ */

static int b64_val(int c)
{
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= 'a' && c <= 'z') return c - 'a' + 26;
  if(c >= '0' && c <= '9') return c - '0' + 52;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

/* returns number of decoded bytes, or -1 on error */
static int b64_decode(const char *in, unsigned char *out, size_t out_max)
{
  size_t n = 0;
  int buf = 0;
  int bits = 0;
  const char *p;
  for(p = in; *p != '\0'; p++)
  {
    int v;
    if(*p == '=' || *p == '\n' || *p == '\r' || *p == ' ')
      continue;
    v = b64_val((unsigned char)*p);
    if(v < 0)
      return -1;
    buf = (buf << 6) | v;
    bits += 6;
    if(bits >= 8)
    {
      bits -= 8;
      if(n >= out_max)
        return -1;
      out[n++] = (unsigned char)((buf >> bits) & 0xff);
    }
  }
  return (int)n;
}

/* ------------------------------------------------------------------ */
/* Minimal JSON parser (objects/arrays/strings/numbers/bool/null)      */
/* ------------------------------------------------------------------ */

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } jtype_t;

typedef struct jnode {
  jtype_t type;
  char *key;              /* member key when inside an object, else NULL */
  char *str;              /* string value / number text */
  double num;
  int bval;
  struct jnode *child;    /* first child (array elements or object members) */
  struct jnode *next;     /* next sibling */
} jnode_t;

typedef struct {
  const char *s;
  size_t pos;
  size_t len;
} jparser_t;

static void json_free(jnode_t *n)
{
  while(n != NULL)
  {
    jnode_t *nx = n->next;
    if(n->child != NULL)
      json_free(n->child);
    free(n->key);
    free(n->str);
    free(n);
    n = nx;
  }
}

static void json_skip_ws(jparser_t *p)
{
  while(p->pos < p->len)
  {
    char c = p->s[p->pos];
    if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
      p->pos++;
    else
      break;
  }
}

static char *json_parse_string(jparser_t *p)
{
  size_t start;
  size_t i;
  char *out;
  size_t oi;
  if(p->pos >= p->len || p->s[p->pos] != '"')
    return NULL;
  p->pos++;
  start = p->pos;
  /* first pass: find end */
  i = start;
  while(i < p->len && p->s[i] != '"')
  {
    if(p->s[i] == '\\')
      i++;
    i++;
  }
  if(i >= p->len)
    return NULL;
  out = (char *)malloc(i - start + 1);
  if(out == NULL)
    return NULL;
  oi = 0;
  while(p->pos < i)
  {
    char c = p->s[p->pos];
    if(c == '\\' && p->pos + 1 < i)
    {
      char e = p->s[p->pos + 1];
      p->pos += 2;
      switch(e)
      {
        case 'n': out[oi++] = '\n'; break;
        case 't': out[oi++] = '\t'; break;
        case 'r': out[oi++] = '\r'; break;
        case '/': out[oi++] = '/'; break;
        case '\\': out[oi++] = '\\'; break;
        case '"': out[oi++] = '"'; break;
        default: out[oi++] = e; break;
      }
    }
    else
    {
      out[oi++] = c;
      p->pos++;
    }
  }
  out[oi] = '\0';
  p->pos = i + 1; /* skip closing quote */
  return out;
}

static jnode_t *json_parse_value(jparser_t *p);

static jnode_t *json_new(jtype_t t)
{
  jnode_t *n = (jnode_t *)calloc(1, sizeof(jnode_t));
  if(n != NULL)
    n->type = t;
  return n;
}

static jnode_t *json_parse_object(jparser_t *p)
{
  jnode_t *obj = json_new(J_OBJ);
  jnode_t *last = NULL;
  if(obj == NULL)
    return NULL;
  p->pos++; /* skip { */
  json_skip_ws(p);
  if(p->pos < p->len && p->s[p->pos] == '}')
  {
    p->pos++;
    return obj;
  }
  while(p->pos < p->len)
  {
    char *key;
    jnode_t *val;
    json_skip_ws(p);
    key = json_parse_string(p);
    if(key == NULL) { json_free(obj); return NULL; }
    json_skip_ws(p);
    if(p->pos >= p->len || p->s[p->pos] != ':') { free(key); json_free(obj); return NULL; }
    p->pos++;
    val = json_parse_value(p);
    if(val == NULL) { free(key); json_free(obj); return NULL; }
    val->key = key;
    if(last == NULL)
      obj->child = val;
    else
      last->next = val;
    last = val;
    json_skip_ws(p);
    if(p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
    if(p->pos < p->len && p->s[p->pos] == '}') { p->pos++; break; }
    json_free(obj);
    return NULL;
  }
  return obj;
}

static jnode_t *json_parse_array(jparser_t *p)
{
  jnode_t *arr = json_new(J_ARR);
  jnode_t *last = NULL;
  if(arr == NULL)
    return NULL;
  p->pos++; /* skip [ */
  json_skip_ws(p);
  if(p->pos < p->len && p->s[p->pos] == ']')
  {
    p->pos++;
    return arr;
  }
  while(p->pos < p->len)
  {
    jnode_t *val = json_parse_value(p);
    if(val == NULL) { json_free(arr); return NULL; }
    if(last == NULL)
      arr->child = val;
    else
      last->next = val;
    last = val;
    json_skip_ws(p);
    if(p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
    if(p->pos < p->len && p->s[p->pos] == ']') { p->pos++; break; }
    json_free(arr);
    return NULL;
  }
  return arr;
}

static jnode_t *json_parse_value(jparser_t *p)
{
  json_skip_ws(p);
  if(p->pos >= p->len)
    return NULL;
  {
    char c = p->s[p->pos];
    if(c == '{')
      return json_parse_object(p);
    if(c == '[')
      return json_parse_array(p);
    if(c == '"')
    {
      jnode_t *n = json_new(J_STR);
      if(n == NULL) return NULL;
      n->str = json_parse_string(p);
      if(n->str == NULL) { free(n); return NULL; }
      return n;
    }
    if(c == 't' || c == 'f')
    {
      jnode_t *n = json_new(J_BOOL);
      if(n == NULL) return NULL;
      if(c == 't') { n->bval = 1; p->pos += 4; }
      else { n->bval = 0; p->pos += 5; }
      return n;
    }
    if(c == 'n')
    {
      jnode_t *n = json_new(J_NULL);
      if(n == NULL) return NULL;
      p->pos += 4;
      return n;
    }
    /* number */
    {
      size_t start = p->pos;
      jnode_t *n;
      char *txt;
      while(p->pos < p->len)
      {
        char d = p->s[p->pos];
        if((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' ||
            d == 'e' || d == 'E')
          p->pos++;
        else
          break;
      }
      n = json_new(J_NUM);
      if(n == NULL) return NULL;
      txt = (char *)malloc(p->pos - start + 1);
      if(txt == NULL) { free(n); return NULL; }
      memcpy(txt, p->s + start, p->pos - start);
      txt[p->pos - start] = '\0';
      n->str = txt;
      n->num = strtod(txt, NULL);
      return n;
    }
  }
}

static jnode_t *json_parse(const char *s, size_t len)
{
  jparser_t p;
  p.s = s;
  p.pos = 0;
  p.len = len;
  return json_parse_value(&p);
}

static jnode_t *json_get(const jnode_t *obj, const char *key)
{
  jnode_t *c;
  if(obj == NULL || obj->type != J_OBJ)
    return NULL;
  for(c = obj->child; c != NULL; c = c->next)
    if(c->key != NULL && strcmp(c->key, key) == 0)
      return c;
  return NULL;
}

static const char *json_str(const jnode_t *obj, const char *key)
{
  jnode_t *n = json_get(obj, key);
  if(n == NULL || (n->type != J_STR && n->type != J_NUM))
    return NULL;
  return n->str;
}

static uint64_t json_u64(const jnode_t *obj, const char *key, uint64_t def)
{
  const char *s = json_str(obj, key);
  jnode_t *n;
  if(s != NULL)
    return strtoull(s, NULL, 10);
  n = json_get(obj, key);
  if(n != NULL && n->type == J_NUM)
    return (uint64_t)n->num;
  return def;
}

/* ------------------------------------------------------------------ */
/* Crypto primitives                                                   */
/* ------------------------------------------------------------------ */

static const EVP_MD *md_by_luks_name(const char *name)
{
  if(name == NULL)
    return NULL;
  if(strcmp(name, "sha1") == 0)      return EVP_sha1();
  if(strcmp(name, "sha256") == 0)    return EVP_sha256();
  if(strcmp(name, "sha512") == 0)    return EVP_sha512();
  if(strcmp(name, "ripemd160") == 0) return EVP_ripemd160();
  return NULL;
}

static int pbkdf2(const EVP_MD *md, const char *pass, size_t passlen,
    const unsigned char *salt, size_t saltlen, unsigned int iter,
    unsigned char *out, size_t outlen)
{
  return PKCS5_PBKDF2_HMAC(pass, (int)passlen, salt, (int)saltlen,
      (int)iter, md, (int)outlen, out) == 1 ? 0 : -1;
}

/*
 * Supported cipher specs. cryptsetup encodes them as
 * "<cipher>-<chainmode>-<ivmode>", e.g. "aes-xts-plain64" or
 * "aes-cbc-essiv:sha256". LUKS1 stores <cipher> and <chainmode>-<ivmode>
 * in separate header fields; we recombine and parse them the same way.
 *
 * Only the "aes" cipher is supported, with chain modes XTS and CBC, and
 * IV modes plain (32-bit), plain64 (64-bit) and essiv:<hash>.
 */
#define CHAIN_XTS 0
#define CHAIN_CBC 1

#define IVM_PLAIN   0
#define IVM_PLAIN64 1
#define IVM_ESSIV   2

typedef struct {
  int chain;
  int ivm;
  const EVP_MD *essiv_md;  /* ESSIV salt hash, else NULL */
} luks_cipher_t;

static int luks_parse_cipher(const char *cipher, const char *mode,
    luks_cipher_t *out)
{
  const char *dash;
  const char *iv;
  char chain[16];
  size_t clen;

  if(cipher == NULL || mode == NULL)
    return -1;
  if(strcmp(cipher, "aes") != 0)
  {
    log_error("luksdec: unsupported cipher '%s' (only aes)\n", cipher);
    return -1;
  }
  dash = strchr(mode, '-');
  if(dash == NULL)
    return -1;
  clen = (size_t)(dash - mode);
  if(clen >= sizeof(chain))
    return -1;
  memcpy(chain, mode, clen);
  chain[clen] = '\0';
  iv = dash + 1;

  out->essiv_md = NULL;
  if(strcmp(chain, "xts") == 0)
    out->chain = CHAIN_XTS;
  else if(strcmp(chain, "cbc") == 0)
    out->chain = CHAIN_CBC;
  else
  {
    log_error("luksdec: unsupported chain mode '%s'\n", chain);
    return -1;
  }

  if(strcmp(iv, "plain") == 0)
    out->ivm = IVM_PLAIN;
  else if(strcmp(iv, "plain64") == 0)
    out->ivm = IVM_PLAIN64;
  else if(strncmp(iv, "essiv:", 6) == 0)
  {
    out->ivm = IVM_ESSIV;
    out->essiv_md = md_by_luks_name(iv + 6);
    if(out->essiv_md == NULL)
    {
      log_error("luksdec: unsupported ESSIV hash '%s'\n", iv + 6);
      return -1;
    }
  }
  else
  {
    log_error("luksdec: unsupported IV mode '%s'\n", iv);
    return -1;
  }
  return 0;
}

static int luks_valid_keylen(int chain, uint64_t keylen)
{
  if(chain == CHAIN_XTS)
    return keylen == 32 || keylen == 64;
  /* CBC: single AES key */
  return keylen == 16 || keylen == 24 || keylen == 32;
}

/* Parse a combined "aes-xts-plain64" style spec (LUKS2 JSON form). */
static int luks_parse_cipher_spec(const char *spec, luks_cipher_t *out)
{
  const char *dash;
  char cipher[LUKS_CIPHERNAME_L + 1];
  char mode[LUKS_CIPHERMODE_L + 1];
  size_t clen;
  size_t mlen;

  if(spec == NULL)
    return -1;
  dash = strchr(spec, '-');
  if(dash == NULL)
    return -1;
  clen = (size_t)(dash - spec);
  mlen = strlen(dash + 1);
  if(clen >= sizeof(cipher) || mlen >= sizeof(mode))
    return -1;
  memset(cipher, 0, sizeof(cipher));
  memcpy(cipher, spec, clen);
  memcpy(mode, dash + 1, mlen + 1);
  return luks_parse_cipher(cipher, mode, out);
}

static int hash_once(const EVP_MD *md, const unsigned char *in, size_t inlen,
    unsigned char *out, unsigned int *outlen)
{
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  int ok;
  if(ctx == NULL)
    return -1;
  ok = EVP_DigestInit_ex(ctx, md, NULL) == 1 &&
       EVP_DigestUpdate(ctx, in, inlen) == 1 &&
       EVP_DigestFinal_ex(ctx, out, outlen) == 1;
  EVP_MD_CTX_free(ctx);
  return ok ? 0 : -1;
}

static int aes_ecb_encrypt_block(const unsigned char *key, size_t keylen,
    const unsigned char *in, unsigned char *out)
{
  const EVP_CIPHER *cipher;
  EVP_CIPHER_CTX *ctx;
  int outl = 0;
  if(keylen == 16)      cipher = EVP_aes_128_ecb();
  else if(keylen == 24) cipher = EVP_aes_192_ecb();
  else if(keylen == 32) cipher = EVP_aes_256_ecb();
  else return -1;
  ctx = EVP_CIPHER_CTX_new();
  if(ctx == NULL)
    return -1;
  EVP_CIPHER_CTX_set_padding(ctx, 0);
  if(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) != 1 ||
     EVP_EncryptUpdate(ctx, out, &outl, in, 16) != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

/*
 * Decrypt a region cipher-block by cipher-block (sector_size bytes each).
 * The tweak/IV for the block at relative byte offset `off` uses sector
 * number (iv_base + off/512), matching dm-crypt semantics.
 *
 * keylen is the full key: for XTS it is 32 (AES-128) or 64 (AES-256);
 * for CBC it is 16/24/32. For ESSIV, the salt key is hash(key).
 */
static int luks_decrypt_region(const luks_cipher_t *c,
    const unsigned char *key, size_t keylen,
    unsigned int sector_size, uint64_t iv_base,
    const unsigned char *in, unsigned char *out, size_t len)
{
  const EVP_CIPHER *cipher;
  size_t off;
  unsigned char essiv_salt[EVP_MAX_MD_SIZE];
  unsigned int essiv_saltlen = 0;

  if(c->chain == CHAIN_XTS)
  {
    if(keylen == 32)      cipher = EVP_aes_128_xts();
    else if(keylen == 64) cipher = EVP_aes_256_xts();
    else return -1;
  }
  else /* CBC */
  {
    if(keylen == 16)      cipher = EVP_aes_128_cbc();
    else if(keylen == 24) cipher = EVP_aes_192_cbc();
    else if(keylen == 32) cipher = EVP_aes_256_cbc();
    else return -1;
  }
  if(sector_size == 0 || (len % sector_size) != 0)
    return -1;

  if(c->ivm == IVM_ESSIV)
  {
    if(hash_once(c->essiv_md, key, keylen, essiv_salt, &essiv_saltlen) != 0)
      return -1;
    if(essiv_saltlen != 16 && essiv_saltlen != 24 && essiv_saltlen != 32)
    {
      log_error("luksdec: ESSIV salt length %u invalid\n", essiv_saltlen);
      return -1;
    }
  }

  for(off = 0; off < len; off += sector_size)
  {
    unsigned char iv[16];
    uint64_t sector = iv_base + (off / 512);
    int outl = 0;
    int finl = 0;
    EVP_CIPHER_CTX *ctx;

    memset(iv, 0, sizeof(iv));
    if(c->ivm == IVM_PLAIN)
    {
      iv[0] = (unsigned char)(sector & 0xff);
      iv[1] = (unsigned char)((sector >> 8) & 0xff);
      iv[2] = (unsigned char)((sector >> 16) & 0xff);
      iv[3] = (unsigned char)((sector >> 24) & 0xff);
    }
    else /* plain64 and essiv seed with 64-bit LE sector */
    {
      int b;
      for(b = 0; b < 8; b++)
        iv[b] = (unsigned char)((sector >> (8 * b)) & 0xff);
    }
    if(c->ivm == IVM_ESSIV)
    {
      unsigned char seed[16];
      memcpy(seed, iv, 16);
      if(aes_ecb_encrypt_block(essiv_salt, essiv_saltlen, seed, iv) != 0)
        return -1;
    }

    ctx = EVP_CIPHER_CTX_new();
    if(ctx == NULL)
      return -1;
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    if(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) != 1 ||
       EVP_DecryptUpdate(ctx, out + off, &outl, in + off, (int)sector_size) != 1 ||
       EVP_DecryptFinal_ex(ctx, out + off + outl, &finl) != 1)
    {
      EVP_CIPHER_CTX_free(ctx);
      return -1;
    }
    EVP_CIPHER_CTX_free(ctx);
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Anti-forensic (AF) splitter merge                                   */
/* ------------------------------------------------------------------ */

static int af_diffuse(const EVP_MD *md, unsigned char *src, size_t size)
{
  unsigned int digest_size = (unsigned int)EVP_MD_size(md);
  size_t nblocks = size / digest_size;
  size_t padding = size % digest_size;
  size_t i;
  unsigned char buf[EVP_MAX_MD_SIZE];
  for(i = 0; i < nblocks; i++)
  {
    unsigned char iv[4];
    EVP_MD_CTX *ctx;
    unsigned int dlen = 0;
    uint32_t idx = (uint32_t)i;
    iv[0] = (unsigned char)((idx >> 24) & 0xff);
    iv[1] = (unsigned char)((idx >> 16) & 0xff);
    iv[2] = (unsigned char)((idx >> 8) & 0xff);
    iv[3] = (unsigned char)(idx & 0xff);
    ctx = EVP_MD_CTX_new();
    if(ctx == NULL) return -1;
    if(EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
       EVP_DigestUpdate(ctx, iv, 4) != 1 ||
       EVP_DigestUpdate(ctx, src + i * digest_size, digest_size) != 1 ||
       EVP_DigestFinal_ex(ctx, src + i * digest_size, &dlen) != 1)
    { EVP_MD_CTX_free(ctx); return -1; }
    EVP_MD_CTX_free(ctx);
  }
  if(padding > 0)
  {
    unsigned char iv[4];
    EVP_MD_CTX *ctx;
    unsigned int dlen = 0;
    uint32_t idx = (uint32_t)nblocks;
    iv[0] = (unsigned char)((idx >> 24) & 0xff);
    iv[1] = (unsigned char)((idx >> 16) & 0xff);
    iv[2] = (unsigned char)((idx >> 8) & 0xff);
    iv[3] = (unsigned char)(idx & 0xff);
    ctx = EVP_MD_CTX_new();
    if(ctx == NULL) return -1;
    if(EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
       EVP_DigestUpdate(ctx, iv, 4) != 1 ||
       EVP_DigestUpdate(ctx, src + nblocks * digest_size, padding) != 1 ||
       EVP_DigestFinal_ex(ctx, buf, &dlen) != 1)
    { EVP_MD_CTX_free(ctx); return -1; }
    EVP_MD_CTX_free(ctx);
    memcpy(src + nblocks * digest_size, buf, padding);
  }
  return 0;
}

static int af_merge(const EVP_MD *md, const unsigned char *src,
    unsigned char *dst, size_t blocksize, unsigned int stripes)
{
  unsigned char *buf = (unsigned char *)calloc(1, blocksize);
  unsigned int i;
  size_t k;
  if(buf == NULL)
    return -1;
  for(i = 0; i < stripes - 1; i++)
  {
    for(k = 0; k < blocksize; k++)
      buf[k] ^= src[i * blocksize + k];
    if(af_diffuse(md, buf, blocksize) != 0)
    {
      free(buf);
      return -1;
    }
  }
  for(k = 0; k < blocksize; k++)
    dst[k] = buf[k] ^ src[(stripes - 1) * blocksize + k];
  free(buf);
  return 0;
}

/* ------------------------------------------------------------------ */
/* Decrypting disk_t wrapper                                           */
/* ------------------------------------------------------------------ */

struct luksdec_data {
  disk_t *base;                 /* borrowed, not owned */
  uint64_t payload_offset;      /* absolute byte offset of encrypted payload */
  unsigned char mk[LUKSDEC_MAX_KEYBYTES];
  size_t mk_len;
  unsigned int enc_sector_size; /* crypto sector size (usually 512) */
  uint64_t iv_tweak;            /* segment iv_tweak base */
  luks_cipher_t cipher;         /* payload cipher spec */
};

static const char *luksdec_description(disk_t *disk)
{
  snprintf(disk->description_txt, sizeof(disk->description_txt),
      "LUKS decrypted %s", disk->device ? disk->device : "");
  return disk->description_txt;
}

static const char *luksdec_description_short(disk_t *disk)
{
  return luksdec_description(disk);
}

static int luksdec_pread(disk_t *disk, void *buf, const unsigned int count,
    const uint64_t offset)
{
  struct luksdec_data *d = (struct luksdec_data *)disk->data;
  unsigned int ss = d->enc_sector_size;
  uint64_t start = offset;
  uint64_t end = offset + count;
  uint64_t astart = (start / ss) * ss;
  uint64_t aend = ((end + ss - 1) / ss) * ss;
  size_t clen = (size_t)(aend - astart);
  unsigned char *cbuf;
  unsigned char *pbuf;
  uint64_t iv_base;

  cbuf = (unsigned char *)malloc(clen);
  if(cbuf == NULL)
    return -1;
  pbuf = (unsigned char *)malloc(clen);
  if(pbuf == NULL) { free(cbuf); return -1; }

  if(base_read(d->base, cbuf, d->payload_offset + astart, clen) != 0)
  {
    free(cbuf); free(pbuf);
    return -1;
  }
  iv_base = d->iv_tweak + (astart / 512);
  if(luks_decrypt_region(&d->cipher, d->mk, d->mk_len, ss, iv_base,
        cbuf, pbuf, clen) != 0)
  {
    free(cbuf); free(pbuf);
    return -1;
  }
  memcpy(buf, pbuf + (start - astart), count);
  free(cbuf);
  free(pbuf);
  return (int)count;
}

static int luksdec_nopwrite(disk_t *disk, const void *buf,
    const unsigned int count, const uint64_t offset)
{
  (void)disk; (void)buf; (void)offset;
  return (int)count;
}

static int luksdec_sync(disk_t *disk)
{
  (void)disk;
  return 0;
}

static void luksdec_clean(disk_t *disk)
{
  if(disk == NULL)
    return;
  if(disk->data != NULL)
  {
    memset(disk->data, 0, sizeof(struct luksdec_data));
    free(disk->data);
    disk->data = NULL;
  }
  free(disk->device);
  free(disk->model);
  free(disk->serial_no);
  free(disk->fw_rev);
  free(disk->rbuffer);
  free(disk->wbuffer);
  free(disk);
}

static disk_t *luksdec_build_disk(disk_t *base, const unsigned char *mk,
    size_t mk_len, uint64_t payload_offset, unsigned int enc_sector_size,
    uint64_t iv_tweak, const luks_cipher_t *cipher)
{
  disk_t *disk;
  struct luksdec_data *d;
  uint64_t payload_size;

  disk = (disk_t *)malloc(sizeof(*disk));
  if(disk == NULL)
    return NULL;
  memset(disk, 0, sizeof(*disk));
  disk->arch = base->arch;
  disk->arch_autodetected = base->arch;
  init_disk(disk);

  d = (struct luksdec_data *)malloc(sizeof(*d));
  if(d == NULL) { free(disk); return NULL; }
  memset(d, 0, sizeof(*d));
  d->base = base;
  d->payload_offset = payload_offset;
  memcpy(d->mk, mk, mk_len);
  d->mk_len = mk_len;
  d->enc_sector_size = enc_sector_size;
  d->iv_tweak = iv_tweak;
  d->cipher = *cipher;

  {
    char name[64];
    snprintf(name, sizeof(name), "LUKS(%s)", base->device ? base->device : "");
    disk->device = strdup(name);
  }
  disk->data = d;
  disk->sector_size = base->sector_size > 0 ? base->sector_size : 512;
  disk->access_mode = TESTDISK_O_RDONLY;
  disk->description = &luksdec_description;
  disk->description_short = &luksdec_description_short;
  disk->pread = &luksdec_pread;
  disk->pwrite = &luksdec_nopwrite;
  disk->sync = &luksdec_sync;
  disk->clean = &luksdec_clean;

  payload_size = base->disk_size > payload_offset ?
    base->disk_size - payload_offset : 0;
  disk->geom.cylinders = 0;
  disk->geom.heads_per_cylinder = 255;
  disk->geom.sectors_per_head = 63;
  disk->disk_real_size = payload_size;
  update_disk_car_fields(disk);
  return disk;
}

/* ------------------------------------------------------------------ */
/* LUKS1 unlock                                                        */
/* ------------------------------------------------------------------ */

static disk_t *luks1_unlock(disk_t *base, uint64_t part_offset,
    const unsigned char *hdr_sector, const char *passphrase)
{
  const struct luks_phdr *hdr = (const struct luks_phdr *)hdr_sector;
  char cipher_name[LUKS_CIPHERNAME_L + 1];
  char cipher_mode[LUKS_CIPHERMODE_L + 1];
  char hash_spec[LUKS_HASHSPEC_L + 1];
  unsigned int key_bytes = be32(hdr->keyBytes);
  unsigned int payload_off_sectors = be32(hdr->payloadOffset);
  const EVP_MD *md;
  luks_cipher_t cipher;
  unsigned int i;
  disk_t *result = NULL;

  memset(cipher_name, 0, sizeof(cipher_name));
  memset(cipher_mode, 0, sizeof(cipher_mode));
  memset(hash_spec, 0, sizeof(hash_spec));
  memcpy(cipher_name, hdr->cipherName, LUKS_CIPHERNAME_L);
  memcpy(cipher_mode, hdr->cipherMode, LUKS_CIPHERMODE_L);
  memcpy(hash_spec, hdr->hashSpec, LUKS_HASHSPEC_L);

  log_info("luks1_unlock: cipher=%s-%s hash=%s keyBytes=%u payloadOffset=%u\n",
      cipher_name, cipher_mode, hash_spec, key_bytes, payload_off_sectors);

  if(luks_parse_cipher(cipher_name, cipher_mode, &cipher) != 0)
    return NULL;
  if(!luks_valid_keylen(cipher.chain, key_bytes))
  {
    log_error("luks1_unlock: unsupported key size %u\n", key_bytes);
    return NULL;
  }
  md = md_by_luks_name(hash_spec);
  if(md == NULL)
  {
    log_error("luks1_unlock: unsupported hash %s\n", hash_spec);
    return NULL;
  }

  for(i = 0; i < LUKS_NUMKEYS && result == NULL; i++)
  {
    const luks_keyslot_t *ks = &hdr->keyslot[i];
    unsigned int active = be32(ks->active);
    unsigned int iterations = be32(ks->passwordIterations);
    unsigned int stripes = be32(ks->stripes);
    unsigned int km_offset = be32(ks->keyMaterialOffset);
    unsigned char derived[LUKSDEC_MAX_KEYBYTES];
    unsigned char *km = NULL;
    unsigned char *split = NULL;
    unsigned char candidate[LUKSDEC_MAX_KEYBYTES];
    unsigned char check[LUKSDEC_MAX_DIGEST];
    size_t km_len;

    if(active != 0x00AC71F3) /* LUKS_KEY_ENABLED */
      continue;

    log_info("luks1_unlock: trying keyslot %u (iter=%u stripes=%u)\n",
        i, iterations, stripes);

    if(pbkdf2(md, passphrase, strlen(passphrase),
          ks->passwordSalt, LUKS_SALTSIZE, iterations,
          derived, key_bytes) != 0)
      continue;

    km_len = (size_t)key_bytes * stripes;
    km = (unsigned char *)malloc(km_len);
    split = (unsigned char *)malloc(km_len);
    if(km == NULL || split == NULL)
    {
      free(km); free(split);
      continue;
    }

    if(base_read(base, km,
          part_offset + (uint64_t)km_offset * LUKS1_SECTOR_SIZE, km_len) != 0)
    {
      free(km); free(split);
      continue;
    }

    /* keyslot area encrypted with the same cipher as the payload, using
     * the derived key, 512-byte sectors, iv seeded from 0 */
    if(luks_decrypt_region(&cipher, derived, key_bytes, LUKS1_SECTOR_SIZE, 0,
          km, split, km_len) != 0)
    {
      free(km); free(split);
      continue;
    }

    if(af_merge(md, split, candidate, key_bytes, stripes) != 0)
    {
      free(km); free(split);
      continue;
    }
    free(km);
    free(split);

    if(pbkdf2(md, (const char *)candidate, key_bytes,
          hdr->mkDigestSalt, LUKS_SALTSIZE, be32(hdr->mkDigestIterations),
          check, LUKS_DIGESTSIZE) != 0)
      continue;

    if(memcmp(check, hdr->mkDigest, LUKS_DIGESTSIZE) == 0)
    {
      log_info("luks1_unlock: keyslot %u matched\n", i);
      result = luksdec_build_disk(base, candidate, key_bytes,
          part_offset + (uint64_t)payload_off_sectors * LUKS1_SECTOR_SIZE,
          LUKS1_SECTOR_SIZE, 0, &cipher);
    }
    memset(candidate, 0, sizeof(candidate));
    memset(derived, 0, sizeof(derived));
  }

  if(result == NULL)
    log_error("luks1_unlock: no keyslot matched (wrong passphrase?)\n");
  return result;
}

/* ------------------------------------------------------------------ */
/* LUKS2 unlock                                                        */
/* ------------------------------------------------------------------ */

static int luks2_derive_key(const jnode_t *kdf, const char *passphrase,
    unsigned char *out, size_t outlen)
{
  const char *type = json_str(kdf, "type");
  const char *salt_b64 = json_str(kdf, "salt");
  unsigned char salt[256];
  int saltlen;

  if(type == NULL || salt_b64 == NULL)
    return -1;
  saltlen = b64_decode(salt_b64, salt, sizeof(salt));
  if(saltlen < 0)
    return -1;

  if(strcmp(type, "pbkdf2") == 0)
  {
    const char *hash = json_str(kdf, "hash");
    unsigned int iter = (unsigned int)json_u64(kdf, "iterations", 0);
    const EVP_MD *md = md_by_luks_name(hash);
    if(md == NULL)
      return -1;
    return pbkdf2(md, passphrase, strlen(passphrase), salt, saltlen,
        iter, out, outlen);
  }
  if(strcmp(type, "argon2i") == 0 || strcmp(type, "argon2id") == 0)
  {
    uint32_t t_cost = (uint32_t)json_u64(kdf, "time", 0);
    uint32_t m_cost = (uint32_t)json_u64(kdf, "memory", 0);
    uint32_t par = (uint32_t)json_u64(kdf, "cpus", 1);
    int rc;
    if(strcmp(type, "argon2id") == 0)
      rc = argon2id_hash_raw(t_cost, m_cost, par, passphrase,
          strlen(passphrase), salt, saltlen, out, outlen);
    else
      rc = argon2i_hash_raw(t_cost, m_cost, par, passphrase,
          strlen(passphrase), salt, saltlen, out, outlen);
    return rc == ARGON2_OK ? 0 : -1;
  }
  log_error("luks2: unsupported kdf type %s\n", type);
  return -1;
}

/* Find the digest object that references the given keyslot id. */
static const jnode_t *luks2_find_digest(const jnode_t *digests,
    const char *keyslot_id)
{
  const jnode_t *dg;
  if(digests == NULL)
    return NULL;
  for(dg = digests->child; dg != NULL; dg = dg->next)
  {
    const jnode_t *kslist = json_get(dg, "keyslots");
    const jnode_t *e;
    if(kslist == NULL || kslist->type != J_ARR)
      continue;
    for(e = kslist->child; e != NULL; e = e->next)
      if(e->type == J_STR && e->str != NULL &&
          strcmp(e->str, keyslot_id) == 0)
        return dg;
  }
  return NULL;
}

static int luks2_verify_digest(const jnode_t *digest,
    const unsigned char *mk, size_t mk_len)
{
  const char *hash = json_str(digest, "hash");
  const char *salt_b64 = json_str(digest, "salt");
  const char *dig_b64 = json_str(digest, "digest");
  unsigned int iter = (unsigned int)json_u64(digest, "iterations", 0);
  unsigned char salt[256];
  unsigned char want[LUKSDEC_MAX_DIGEST];
  unsigned char got[LUKSDEC_MAX_DIGEST];
  int saltlen, diglen;
  const EVP_MD *md = md_by_luks_name(hash);

  if(md == NULL || salt_b64 == NULL || dig_b64 == NULL)
    return -1;
  saltlen = b64_decode(salt_b64, salt, sizeof(salt));
  diglen = b64_decode(dig_b64, want, sizeof(want));
  if(saltlen < 0 || diglen < 0)
    return -1;
  if(pbkdf2(md, (const char *)mk, mk_len, salt, saltlen, iter,
        got, (size_t)diglen) != 0)
    return -1;
  return memcmp(got, want, diglen) == 0 ? 0 : -1;
}

static disk_t *luks2_unlock(disk_t *base, uint64_t part_offset,
    const unsigned char *bin_hdr, const char *passphrase)
{
  uint64_t hdr_size;
  char *json_text = NULL;
  size_t json_len;
  jnode_t *root = NULL;
  const jnode_t *keyslots, *digests, *segments;
  const jnode_t *ks;
  disk_t *result = NULL;

  hdr_size = be64(*(const uint64_t *)(bin_hdr + 8));
  log_info("luks2_unlock: hdr_size=%llu\n", (unsigned long long)hdr_size);
  if(hdr_size <= 4096 || hdr_size > LUKSDEC_MAX_JSON)
  {
    log_error("luks2_unlock: implausible hdr_size %llu\n",
        (unsigned long long)hdr_size);
    return NULL;
  }
  json_len = (size_t)(hdr_size - 4096);
  json_text = (char *)malloc(json_len + 1);
  if(json_text == NULL)
    return NULL;
  if(base_read(base, json_text, part_offset + 4096, json_len) != 0)
  {
    free(json_text);
    return NULL;
  }
  json_text[json_len] = '\0';

  root = json_parse(json_text, json_len);
  free(json_text);
  if(root == NULL || root->type != J_OBJ)
  {
    log_error("luks2_unlock: JSON parse failed\n");
    json_free(root);
    return NULL;
  }

  keyslots = json_get(root, "keyslots");
  digests = json_get(root, "digests");
  segments = json_get(root, "segments");
  if(keyslots == NULL || digests == NULL || segments == NULL)
  {
    log_error("luks2_unlock: missing keyslots/digests/segments\n");
    json_free(root);
    return NULL;
  }

  for(ks = keyslots->child; ks != NULL && result == NULL; ks = ks->next)
  {
    const char *ks_type = json_str(ks, "type");
    const jnode_t *kdf = json_get(ks, "kdf");
    const jnode_t *af = json_get(ks, "af");
    const jnode_t *area = json_get(ks, "area");
    const char *af_hash;
    const char *area_enc;
    unsigned int stripes;
    uint64_t key_size = json_u64(ks, "key_size", 0);
    uint64_t area_offset;
    uint64_t area_key_size;
    const EVP_MD *af_md;
    luks_cipher_t area_cipher;
    unsigned char derived[LUKSDEC_MAX_KEYBYTES];
    unsigned char *km = NULL;
    unsigned char *split = NULL;
    unsigned char candidate[LUKSDEC_MAX_KEYBYTES];
    size_t km_len;
    const jnode_t *digest;

    if(ks_type == NULL || strcmp(ks_type, "luks2") != 0)
      continue;
    if(kdf == NULL || af == NULL || area == NULL)
      continue;

    af_hash = json_str(af, "hash");
    stripes = (unsigned int)json_u64(af, "stripes", 0);
    area_enc = json_str(area, "encryption");
    area_offset = json_u64(area, "offset", 0);
    area_key_size = json_u64(area, "key_size", key_size);

    if(luks_parse_cipher_spec(area_enc, &area_cipher) != 0)
    {
      log_error("luks2_unlock: unsupported keyslot area cipher %s\n",
          area_enc ? area_enc : "(null)");
      continue;
    }
    if(key_size == 0 || key_size > LUKSDEC_MAX_KEYBYTES)
      continue;
    if(!luks_valid_keylen(area_cipher.chain, area_key_size))
      continue;
    af_md = md_by_luks_name(af_hash);
    if(af_md == NULL || stripes == 0)
      continue;

    log_info("luks2_unlock: keyslot key='%s' key_size=%llu stripes=%u\n",
        ks->key ? ks->key : "?", (unsigned long long)key_size, stripes);

    if(luks2_derive_key(kdf, passphrase, derived, (size_t)area_key_size) != 0)
    {
      log_error("luks2_unlock: key derivation failed\n");
      continue;
    }

    km_len = (size_t)key_size * stripes;
    km = (unsigned char *)malloc(km_len);
    split = (unsigned char *)malloc(km_len);
    if(km == NULL || split == NULL) { free(km); free(split); continue; }

    if(base_read(base, km, part_offset + area_offset, km_len) != 0)
    {
      free(km); free(split);
      continue;
    }
    if(luks_decrypt_region(&area_cipher, derived, (size_t)area_key_size,
          LUKS1_SECTOR_SIZE, 0, km, split, km_len) != 0)
    {
      free(km); free(split);
      continue;
    }
    if(af_merge(af_md, split, candidate, (size_t)key_size, stripes) != 0)
    {
      free(km); free(split);
      continue;
    }
    free(km);
    free(split);

    digest = luks2_find_digest(digests, ks->key);
    if(digest == NULL)
    {
      log_error("luks2_unlock: no digest for keyslot %s\n",
          ks->key ? ks->key : "?");
      memset(candidate, 0, sizeof(candidate));
      continue;
    }

    if(luks2_verify_digest(digest, candidate, (size_t)key_size) == 0)
    {
      const jnode_t *seglist = json_get(digest, "segments");
      const char *seg_id = NULL;
      const jnode_t *seg;
      if(seglist != NULL && seglist->type == J_ARR &&
          seglist->child != NULL && seglist->child->type == J_STR)
        seg_id = seglist->child->str;
      seg = seg_id ? json_get(segments, seg_id) : segments->child;
      if(seg != NULL)
      {
        const char *seg_enc = json_str(seg, "encryption");
        uint64_t seg_off = json_u64(seg, "offset", 0);
        unsigned int seg_ss = (unsigned int)json_u64(seg, "sector_size", 512);
        uint64_t iv_tweak = json_u64(seg, "iv_tweak", 0);
        luks_cipher_t seg_cipher;
        if(luks_parse_cipher_spec(seg_enc, &seg_cipher) == 0 &&
            luks_valid_keylen(seg_cipher.chain, key_size))
        {
          log_info("luks2_unlock: matched; segment %s offset=%llu ss=%u\n",
              seg_enc, (unsigned long long)seg_off, seg_ss);
          result = luksdec_build_disk(base, candidate, (size_t)key_size,
              part_offset + seg_off, seg_ss, iv_tweak, &seg_cipher);
        }
        else
        {
          log_error("luks2_unlock: unsupported segment cipher %s\n",
              seg_enc ? seg_enc : "(null)");
        }
      }
    }
    memset(candidate, 0, sizeof(candidate));
    memset(derived, 0, sizeof(derived));
  }

  json_free(root);
  if(result == NULL)
    log_error("luks2_unlock: no keyslot matched (wrong passphrase?)\n");
  return result;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

disk_t *luksdec_open(disk_t *base, uint64_t part_offset, const char *passphrase)
{
  unsigned char *hdr;
  uint16_t version;
  disk_t *result = NULL;

  if(base == NULL || passphrase == NULL)
    return NULL;

  hdr = (unsigned char *)malloc(4096);
  if(hdr == NULL)
    return NULL;
  if(base_read(base, hdr, part_offset, 4096) != 0)
  {
    log_error("luksdec_open: failed to read header at %llu\n",
        (unsigned long long)part_offset);
    free(hdr);
    return NULL;
  }
  if(memcmp(hdr, LUKS2_MAGIC_PRIMARY, 6) != 0)
  {
    log_error("luksdec_open: no LUKS magic at %llu\n",
        (unsigned long long)part_offset);
    free(hdr);
    return NULL;
  }
  version = be16(*(uint16_t *)(hdr + 6));
  log_info("luksdec_open: LUKS version %u at offset %llu\n",
      version, (unsigned long long)part_offset);

  if(version == 1)
    result = luks1_unlock(base, part_offset, hdr, passphrase);
  else if(version == 2)
    result = luks2_unlock(base, part_offset, hdr, passphrase);
  else
    log_error("luksdec_open: unknown LUKS version %u\n", version);

  free(hdr);
  return result;
}
