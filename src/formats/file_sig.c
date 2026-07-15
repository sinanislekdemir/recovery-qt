/*

    File: file_sig.c

    Copyright (C) 2010 Christophe GRENIER <grenier@cgsecurity.org>
  
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
#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_sig)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if defined(__FRAMAC__)
#include "__fc_builtin.h"
#endif
#include "types.h"
#include "filegen.h"
#include "common.h"
#include "log.h"

static int signature_cmp(const struct td_list_head *a, const struct td_list_head *b);

#ifndef __FRAMAC__
#include "list_add_sorted.h"
#else

static inline void td_list_add_sorted_sig(struct td_list_head *newe, struct td_list_head *head)
{
  struct td_list_head *pos;
  
  td_list_for_each(pos, head)
  {
    
    
    if(signature_cmp(newe,pos)<0)
      break;
  }
  if(pos != head)
  {
      __td_list_add(newe, pos->prev, pos);
  }
  else
  {
    
    
    
    
    td_list_add_tail(newe, head);
  }
}
#endif

#if 0

static char *td_strdup(const char *s)
{
  size_t l = strlen(s) + 1;
  char *p = (char *)MALLOC(l);
  
  memcpy(p, s, l);
  p[l-1]='\0';
  
  return p;
}
#endif


static void register_header_check_sig(file_stat_t *file_stat);

const file_hint_t file_hint_sig= {
  .extension="custom",
  .description="Own custom signatures",
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_sig
};

#define WIN_PHOTOREC_SIG "\\photorec.sig"
#define DOT_PHOTOREC_SIG "/.photorec.sig"
#define PHOTOREC_SIG "photorec.sig"

typedef struct signature_s signature_t;
struct signature_s
{
  struct td_list_head list;
  const char *extension;
  const char *sig;
  unsigned int sig_size;
  unsigned int offset;
};



static signature_t signatures={
  .list = TD_LIST_HEAD_INIT(signatures.list)
};


static int signature_cmp(const struct td_list_head *a, const struct td_list_head *b)
{
  const signature_t *sig_a=td_list_entry_const(a, const signature_t, list);
  const signature_t *sig_b=td_list_entry_const(b, const signature_t, list);
  int res;
  
  
  
  
  if(sig_a->sig_size==0 && sig_b->sig_size!=0)
    return -1;
  if(sig_a->sig_size!=0 && sig_b->sig_size==0)
    return 1;
  
  
  res=(int)sig_a->offset - (int)sig_b->offset;
  if(res!=0)
    return res;
  if(sig_a->sig_size<=sig_b->sig_size)
  {
    res=memcmp(sig_a->sig,sig_b->sig, sig_a->sig_size);
    if(res!=0)
      return res;
    return 1;
  }
  else
  {
    res=memcmp(sig_a->sig,sig_b->sig, sig_b->sig_size);
    if(res!=0)
      return res;
    return -1;
  }
}


// requires \initialized((const char *)sig + (0 .. sig_size-1));
static void signature_insert(const char *ext, const unsigned int ext_size, const unsigned int offset, const void*sig, const unsigned int sig_size)
{
  /* FIXME: memory leak for newsig */
  signature_t *newsig;
  
  char *my_ext=(char *)MALLOC(ext_size+1);
  memcpy(my_ext, ext, ext_size);
  my_ext[ext_size]='\0';
  
  
  newsig=(signature_t*)MALLOC(sizeof(*newsig));
  
  newsig->extension=my_ext;
  newsig->sig=(const char *)sig;
  newsig->sig_size=sig_size;
  newsig->offset=offset;
  

  
  
  
  
  
  
  
  
  
  
  
  

  
#ifdef __FRAMAC__
  td_list_add_sorted_sig(&newsig->list, &signatures.list);
#else
  td_list_add_sorted(&newsig->list, &signatures.list, signature_cmp);
#endif
}


static int header_check_sig(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  struct td_list_head *pos;
  
  td_list_for_each(pos, &signatures.list)
  {
    const signature_t *sig = td_list_entry(pos, signature_t, list);
    
    
    
    
    if(memcmp(&buffer[sig->offset], sig->sig, sig->sig_size)==0)
    {
      reset_file_recovery(file_recovery_new);
      file_recovery_new->extension=sig->extension;
      
      return 1;
    }
  }
  return 0;
}

static FILE *open_signature_file(void)
{
#if defined(__CYGWIN__) || defined(__MINGW32__)
  {
    char *path;
    path = getenv("USERPROFILE");
    if (path == NULL)
      path = getenv("HOMEPATH");
    if(path!=NULL)
    {
      FILE*handle;
      char *filename=NULL;
      filename=(char*)MALLOC(strlen(path)+strlen(WIN_PHOTOREC_SIG)+1);
      strcpy(filename, path);
      strcat(filename, WIN_PHOTOREC_SIG);
      handle=fopen(filename,"rb");
      if(handle!=NULL)
      {
	log_info("Open signature file %s\n", filename);
	free(filename);
	return handle;
      }
      free(filename);
    }
  }
#endif
#ifndef DJGPP
  {
    const char *home = getenv("HOME");
    if (home != NULL)
    {
      FILE*handle;
      char *filename;
      size_t len_home;
      const size_t len_sig=strlen(DOT_PHOTOREC_SIG);
      size_t fn_size=len_sig;
#ifndef DISABLED_FOR_FRAMAC
      len_home=strlen(home);
      fn_size+=len_home;
#endif
      filename=(char*)MALLOC(fn_size + 1);
#ifndef DISABLED_FOR_FRAMAC
      strcpy(filename, home);
#else
      filename[0]='\0';
#endif
      strcat(filename, DOT_PHOTOREC_SIG);
      handle=fopen(filename,"rb");
      if(handle!=NULL)
      {
#ifndef DISABLED_FOR_FRAMAC
	log_info("Open signature file %s\n", filename);
#endif
	free(filename);
	return handle;
      }
      free(filename);
    }
  }
#endif
  {
    FILE *handle=fopen(PHOTOREC_SIG,"rb");
    if(handle!=NULL)
    {
#ifndef DISABLED_FOR_FRAMAC
      log_info("Open signature file %s\n", PHOTOREC_SIG);
#endif
      return handle;
    }
  }
  return NULL;
}


static unsigned int str_uint_hex(const char **ptr)
{
  const char *src=*ptr;
  unsigned int res=0;
  
  for(;;src++)
  {
    char c=*src;
    if(c>='0' && c<='9')
      c-='0';
    else if(c>='A' && c<='F')
      c-='A';
    else if(c>='a' && c<='f')
      c-='a';
    else
    {
      
      
      *ptr=src;
      return res;
    }
    
    res=res*16+c;
    if(res >= 0x10000000)
    {
      
      
      *ptr=src;
      return res;
    }
  }
}


static unsigned int str_uint_dec(const char **ptr)
{
  const char *src=*ptr;
  unsigned int res=0;
  
  for(;*src>='0' && *src<='9';src++)
  {
    res=res*10+(*src)-'0';
    if(res >= 0x10000000)
    {
      *ptr=src;
      return res;
    }
  }
  
  
  *ptr=src;
  return res;
}


static unsigned int str_uint(const char **ptr)
{
  const char *src=*ptr;
  unsigned int res;
  
  
  if(*src=='0' && (*(src+1)=='x' || *(src+1)=='X'))
  {
    (*ptr)+=2;
    res=str_uint_hex(ptr);
    
    return res;
  }
  res=str_uint_dec(ptr);
  
  return res;
}


static unsigned char escaped_char(const unsigned char c)
{
  switch(c)
  {
    case 'b':
      return '\b';
    case 'n':
      return '\n';
    case 't':
      return '\t';
    case 'r':
      return '\r';
    case '0':
      return '\0';
    default:
      return c;
  }
}


static unsigned int load_hex1(const unsigned char c)
{
  if(c>='0' && c<='9')
    return c-'0';
  else if(c>='A' && c<='F')
    return c-'A'+10;
  else if(c>='a' && c<='f')
    return c-'a'+10;
  return 0x10;
}


static unsigned int load_hex2(const unsigned char c1, const unsigned char c2)
{
  unsigned int val1=load_hex1(c1);
  unsigned int val2=load_hex1(c2);
  if(val1 >= 0x10 || val2 >=0x10)
    return 0x100;
  return (val1*16)+val2;
}


static unsigned int load_signature(const char **ptr, unsigned char *tmp)
{
  unsigned int signature_size=0;
  const char *pos=*ptr;
  
  
  while(*pos!='\n' && *pos!='\0')
  {
    if(signature_size>=PHOTOREC_MAX_SIG_SIZE)
      return 0;
    
    if(*pos ==' ' || *pos=='\t' || *pos=='\r' || *pos==',')
    {
      pos++;
      
    }
    else if(*pos== '\'')
    {
      pos++;
      
      
      if(*pos=='\0')
	return 0;
      
      if(*pos=='\\')
      {
	pos++;
	
	
	if(*pos=='\0')
	  return 0;
	
	tmp[signature_size++]=escaped_char(*(const unsigned char *)pos);
	
	
      }
      else
      {
	
	
	tmp[signature_size++]=*(const unsigned char *)pos;
	
	
      }
      
      
      
      
      
      
      
      pos++;
      
      
      if(*pos!='\'')
	return 0;
      pos++;
      
    }
#ifndef DISABLED_FOR_FRAMAC
    else if(*pos=='"')
    {
      pos++;
      
      while(*pos!='"')
      {
	if(*pos=='\0')
	  return 0;
	if(signature_size>=PHOTOREC_MAX_SIG_SIZE)
	  return 0;
	if(*pos=='\\')
	{
	  pos++;
	  
	  if(*pos=='\0')
	    return 0;
	  tmp[signature_size++]=escaped_char(*(const unsigned char *)pos);
	}
	else
	  tmp[signature_size++]=*(const unsigned char *)pos;
	pos++;
	
      }
      
      pos++;
      
    }
    else if(*pos=='0' && (*(pos+1)=='x' || *(pos+1)=='X'))
    {
      pos+=2;
      
      
      while(
#ifdef DISABLED_FOR_FRAMAC
	  *pos!='\0' && *(pos+1)!='\0'
#else
	  isxdigit(*pos) && isxdigit(*(pos+1))
#endif
	  )
      {
	unsigned int val;
	if(signature_size>=PHOTOREC_MAX_SIG_SIZE)
	  return 0;
	
	
	val=load_hex2(*(const unsigned char *)pos, *(const unsigned char *)(pos+1));
	if(val >= 0x100)
	  break;
	
	pos+=2;
	
	tmp[signature_size++]=val;
      }
    }
#endif
    else
    {
      return 0;
    }
    
  }
  
  *ptr=pos;
  return signature_size;
}


static const char *parse_signature_line(file_stat_t *file_stat, const char *pos)
{
  /* each line is composed of "extension sig_offset signature" */
  const char *ext=pos;
  unsigned char *sig_sig=NULL;
  unsigned int ext_size=0;
  unsigned int sig_offset=0;
  unsigned int sig_size;
  /* Read the extension */
  
  while(*pos!=' ' && *pos!='\t')
  {
    if(*pos=='\0' || *pos=='\n' || *pos=='\r')
      return pos;
    ext_size++;
    pos++;
  }
  
  
  
//  *pos='\0';
  
  pos++;
  
  
#ifndef DISABLED_FOR_FRAMAC
//  log_info("register a signature for %s\n", ext);
#endif
  /* skip spaces */
  
  while(*pos=='\t' || *pos==' ')
  {
    
    
    pos++;
  }
  
  
  sig_offset=str_uint(&pos);
  
  
  if(sig_offset > PHOTOREC_MAX_SIG_OFFSET)
  {
    /* Invalid sig_offset */
    return pos;
  }
  
  
  /* read signature */
  sig_sig=(unsigned char *)MALLOC(PHOTOREC_MAX_SIG_SIZE);
  
  
  sig_size=load_signature(&pos, sig_sig);
  
  
  
  
  if(sig_size==0)
  {
    free(sig_sig);
    return pos;
  }
  if(*pos=='\n')
    pos++;
  
  
  if(sig_size>0 && sig_offset + sig_size <= PHOTOREC_MAX_SIG_OFFSET )
  {
    /* FIXME: memory leak for signature */
    char *signature;
    
    
    signature=(char*)MALLOC(sig_size);
    
    
    
    memcpy(signature, sig_sig, sig_size);
    
    // TODO assert \initialized(signature + (0 .. sig_size - 1));
    signature_insert(ext, ext_size, sig_offset, signature, sig_size);
#ifndef DISABLED_FOR_FRAMAC
    register_header_check(sig_offset, signature, sig_size, &header_check_sig, file_stat);
#endif
  }
  free(sig_sig);
  
  
  return pos;
}


static const char *parse_signature_file(file_stat_t *file_stat, const char *pos)
{
  
  while(*pos!='\0')
  {
    /* skip comments */
    
    while(*pos=='#')
    {
      
      while(*pos!='\0' && *pos!='\n')
	pos++;
      if(*pos=='\0')
	return pos;
      
      pos++;
    }
    /* skip empty lines */
    
    while(*pos=='\n' || *pos=='\r')
      pos++;
    pos=parse_signature_line(file_stat, pos);
  }
  return pos;
}

static void register_header_check_sig(file_stat_t *file_stat)
{
  const char *pos;
  static char *buffer=NULL;
  size_t buffer_size;
  struct stat stat_rec;
  FILE *handle;
//  if(!td_list_empty(&signatures.list))
  if(buffer!=NULL)
    return ;
  handle=open_signature_file();
  if(!handle)
    return;
#ifdef DISABLED_FOR_FRAMAC
  buffer_size=1024*1024;
#else
  if(fstat(fileno(handle), &stat_rec)<0 || stat_rec.st_size>100*1024*1024)
  {
    fclose(handle);
    return;
  }
  buffer_size=stat_rec.st_size;
#endif
  
  buffer=(char *)MALLOC(buffer_size+1);
  if(fread(buffer,1,buffer_size,handle)!=buffer_size)
  {
    fclose(handle);
    free(buffer);
    return;
  }
  fclose(handle);
#if defined(__FRAMAC__)
  Frama_C_make_unknown(buffer, buffer_size);
#endif
  buffer[buffer_size]='\0';
  pos=buffer;
  pos=parse_signature_file(file_stat, pos);
  if(*pos!='\0')
  {
#ifndef DISABLED_FOR_FRAMAC
    log_warning("Can't parse signature: %s\n", pos);
#endif
  }
  free(buffer);
}
#endif
