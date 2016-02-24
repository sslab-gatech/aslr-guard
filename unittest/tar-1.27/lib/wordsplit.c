/* wordsplit - a word splitter
   Copyright (C) 2009-2013 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/>.

   Written by Sergey Poznyakoff
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#if ENABLE_NLS
# include <gettext.h>
#else
# define gettext(msgid) msgid
#endif
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#include <wordsplit.h>

#define ISWS(c) ((c)==' '||(c)=='\t'||(c)=='\n')
#define ISDELIM(ws,c) \
  (strchr ((ws)->ws_delim, (c)) != NULL)
#define ISPUNCT(c) (strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",(c))!=NULL)
#define ISUPPER(c) ('A' <= ((unsigned) (c)) && ((unsigned) (c)) <= 'Z')
#define ISLOWER(c) ('a' <= ((unsigned) (c)) && ((unsigned) (c)) <= 'z')
#define ISALPHA(c) (ISUPPER(c) || ISLOWER(c))
#define ISDIGIT(c) ('0' <= ((unsigned) (c)) && ((unsigned) (c)) <= '9')
#define ISXDIGIT(c) (strchr("abcdefABCDEF", c)!=NULL)
#define ISALNUM(c) (ISALPHA(c) || ISDIGIT(c))
#define ISPRINT(c) (' ' <= ((unsigned) (c)) && ((unsigned) (c)) <= 127)

#define ALLOC_INIT 128
#define ALLOC_INCR 128

static void
_wsplt_alloc_die (struct wordsplit *wsp)
{
  wsp->ws_error (_("memory exhausted"));
  abort ();
}

static void __attribute__ ((__format__ (__printf__, 1, 2)))
_wsplt_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
}

static void wordsplit_free_nodes (struct wordsplit *);

static int
_wsplt_nomem (struct wordsplit *wsp)
{
  errno = ENOMEM;
  wsp->ws_errno = WRDSE_NOSPACE;
  if (wsp->ws_flags & WRDSF_ENOMEMABRT)
    wsp->ws_alloc_die (wsp);
  if (wsp->ws_flags & WRDSF_SHOWERR)
    wordsplit_perror (wsp);
  if (!(wsp->ws_flags & WRDSF_REUSE))
    wordsplit_free (wsp);
  wordsplit_free_nodes (wsp);
  return wsp->ws_errno;
}

static void
wordsplit_init0 (struct wordsplit *wsp)
{
  if (wsp->ws_flags & WRDSF_REUSE)
    {
      if (!(wsp->ws_flags & WRDSF_APPEND))
	wordsplit_free_words (wsp);
    }
  else
    {
      wsp->ws_wordv = NULL;
      wsp->ws_wordc = 0;
      wsp->ws_wordn = 0;
    }

  wsp->ws_errno = 0;
  wsp->ws_head = wsp->ws_tail = NULL;
}

static int
wordsplit_init (struct wordsplit *wsp, const char *input, size_t len,
		int flags)
{
  wsp->ws_flags = flags;

  if (!(wsp->ws_flags & WRDSF_ALLOC_DIE))
    wsp->ws_alloc_die = _wsplt_alloc_die;
  if (!(wsp->ws_flags & WRDSF_ERROR))
    wsp->ws_error = _wsplt_error;

  if (!(wsp->ws_flags & WRDSF_NOVAR)
      && !(wsp->ws_flags & (WRDSF_ENV | WRDSF_GETVAR)))
    {
      errno = EINVAL;
      wsp->ws_errno = WRDSE_USAGE;
      if (wsp->ws_flags & WRDSF_SHOWERR)
	wordsplit_perror (wsp);
      return wsp->ws_errno;
    }

  if (!(wsp->ws_flags & WRDSF_NOCMD))
    {
      errno = EINVAL;
      wsp->ws_errno = WRDSE_NOSUPP;
      if (wsp->ws_flags & WRDSF_SHOWERR)
	wordsplit_perror (wsp);
      return wsp->ws_errno;
    }

  if (wsp->ws_flags & WRDSF_SHOWDBG)
    {
      if (!(wsp->ws_flags & WRDSF_DEBUG))
	{
	  if (wsp->ws_flags & WRDSF_ERROR)
	    wsp->ws_debug = wsp->ws_error;
	  else if (wsp->ws_flags & WRDSF_SHOWERR)
	    wsp->ws_debug = _wsplt_error;
	  else
	    wsp->ws_flags &= ~WRDSF_SHOWDBG;
	}
    }

  wsp->ws_input = input;
  wsp->ws_len = len;

  if (!(wsp->ws_flags & WRDSF_DOOFFS))
    wsp->ws_offs = 0;

  if (!(wsp->ws_flags & WRDSF_DELIM))
    wsp->ws_delim = " \t\n";

  if (!(wsp->ws_flags & WRDSF_COMMENT))
    wsp->ws_comment = NULL;

  if (!(wsp->ws_flags & WRDSF_CLOSURE))
    wsp->ws_closure = NULL;

  wsp->ws_endp = 0;

  wordsplit_init0 (wsp);

  return 0;
}

static int
alloc_space (struct wordsplit *wsp, size_t count)
{
  size_t offs = (wsp->ws_flags & WRDSF_DOOFFS) ? wsp->ws_offs : 0;
  char **ptr;
  size_t newalloc;

  if (wsp->ws_wordv == NULL)
    {
      newalloc = offs + count > ALLOC_INIT ? count : ALLOC_INIT;
      ptr = calloc (newalloc, sizeof (ptr[0]));
    }
  else if (wsp->ws_wordn < offs + wsp->ws_wordc + count)
    {
      newalloc = offs + wsp->ws_wordc +
	(count > ALLOC_INCR ? count : ALLOC_INCR);
      ptr = realloc (wsp->ws_wordv, newalloc * sizeof (ptr[0]));
    }
  else
    return 0;

  if (ptr)
    {
      wsp->ws_wordn = newalloc;
      wsp->ws_wordv = ptr;
    }
  else
    return _wsplt_nomem (wsp);
  return 0;
}


/* Node state flags */
#define _WSNF_NULL     0x01	/* null node (a noop) */
#define _WSNF_WORD     0x02	/* node contains word in v.word */
#define _WSNF_QUOTE    0x04	/* text is quoted */
#define _WSNF_NOEXPAND 0x08	/* text is not subject to expansion */
#define _WSNF_JOIN     0x10	/* node must be joined with the next node */
#define _WSNF_SEXP     0x20	/* is a sed expression */

#define _WSNF_EMPTYOK  0x0100	/* special flag indicating that
				   wordsplit_add_segm must add the
				   segment even if it is empty */

struct wordsplit_node
{
  struct wordsplit_node *prev;	/* Previous element */
  struct wordsplit_node *next;	/* Next element */
  int flags;			/* Node flags */
  union
  {
    struct
    {
      size_t beg;		/* Start of word in ws_input */
      size_t end;		/* End of word in ws_input */
    } segm;
    char *word;
  } v;
};

static const char *
wsnode_flagstr (int flags)
{
  static char retbuf[6];
  char *p = retbuf;

  if (flags & _WSNF_WORD)
    *p++ = 'w';
  else if (flags & _WSNF_NULL)
    *p++ = 'n';
  else
    *p++ = '-';
  if (flags & _WSNF_QUOTE)
    *p++ = 'q';
  else
    *p++ = '-';
  if (flags & _WSNF_NOEXPAND)
    *p++ = 'E';
  else
    *p++ = '-';
  if (flags & _WSNF_JOIN)
    *p++ = 'j';
  else
    *p++ = '-';
  if (flags & _WSNF_SEXP)
    *p++ = 's';
  else
    *p++ = '-';
  *p = 0;
  return retbuf;
}

static const char *
wsnode_ptr (struct wordsplit *wsp, struct wordsplit_node *p)
{
  if (p->flags & _WSNF_NULL)
    return "";
  else if (p->flags & _WSNF_WORD)
    return p->v.word;
  else
    return wsp->ws_input + p->v.segm.beg;
}

static size_t
wsnode_len (struct wordsplit_node *p)
{
  if (p->flags & _WSNF_NULL)
    return 0;
  else if (p->flags & _WSNF_WORD)
    return strlen (p->v.word);
  else
    return p->v.segm.end - p->v.segm.beg;
}

static int
wsnode_new (struct wordsplit *wsp, struct wordsplit_node **pnode)
{
  struct wordsplit_node *node = calloc (1, sizeof (*node));
  if (!node)
    return _wsplt_nomem (wsp);
  *pnode = node;
  return 0;
}

static void
wsnode_free (struct wordsplit_node *p)
{
  if (p->flags & _WSNF_WORD)
    free (p->v.word);
  free (p);
}

static void
wsnode_append (struct wordsplit *wsp, struct wordsplit_node *node)
{
  node->next = NULL;
  node->prev = wsp->ws_tail;
  if (wsp->ws_tail)
    wsp->ws_tail->next = node;
  else
    wsp->ws_head = node;
  wsp->ws_tail = node;
}

static void
wsnode_remove (struct wordsplit *wsp, struct wordsplit_node *node)
{
  struct wordsplit_node *p;

  p = node->prev;
  if (p)
    {
      p->next = node->next;
      if (!node->next)
	p->flags &= ~_WSNF_JOIN;
    }
  else
    wsp->ws_head = node->next;

  p = node->next;
  if (p)
    p->prev = node->prev;
  else
    wsp->ws_tail = node->prev;

  node->next = node->prev = NULL;
}

static void
wsnode_insert (struct wordsplit *wsp, struct wordsplit_node *node,
	       struct wordsplit_node *anchor, int before)
{
  if (!wsp->ws_head)
    {
      node->next = node->prev = NULL;
      wsp->ws_head = wsp->ws_tail = node;
    }
  else if (before)
    {
      if (anchor->prev)
	wsnode_insert (wsp, node, anchor->prev, 0);
      else
	{
	  node->prev = NULL;
	  node->next = anchor;
	  anchor->prev = node;
	  wsp->ws_head = node;
	}
    }
  else
    {
      struct wordsplit_node *p;

      p = anchor->next;
      if (p)
	p->prev = node;
      else
	wsp->ws_tail = node;
      node->next = p;
      node->prev = anchor;
      anchor->next = node;
    }
}

static int
wordsplit_add_segm (struct wordsplit *wsp, size_t beg, size_t end, int flg)
{
  struct wordsplit_node *node;
  int rc;

  if (end == beg && !(flg & _WSNF_EMPTYOK))
    return 0;
  rc = wsnode_new (wsp, &node);
  if (rc)
    return rc;
  node->flags = flg & ~(_WSNF_WORD | _WSNF_EMPTYOK);
  node->v.segm.beg = beg;
  node->v.segm.end = end;
  wsnode_append (wsp, node);
  return 0;
}

static void
wordsplit_free_nodes (struct wordsplit *wsp)
{
  struct wordsplit_node *p;

  for (p = wsp->ws_head; p;)
    {
      struct wordsplit_node *next = p->next;
      wsnode_free (p);
      p = next;
    }
  wsp->ws_head = wsp->ws_tail = NULL;
}

static void
wordsplit_dump_nodes (struct wordsplit *wsp)
{
  struct wordsplit_node *p;
  int n = 0;

  for (p = wsp->ws_head, n = 0; p; p = p->next, n++)
    {
      if (p->flags & _WSNF_WORD)
	wsp->ws_debug ("%4d: %p: %#04x (%s):%s;",
		       n, p, p->flags, wsnode_flagstr (p->flags), p->v.word);
      else
	wsp->ws_debug ("%4d: %p: %#04x (%s):%.*s;",
		       n, p, p->flags, wsnode_flagstr (p->flags),
		       (int) (p->v.segm.end - p->v.segm.beg),
		       wsp->ws_input + p->v.segm.beg);
    }
}

static int
coalesce_segment (struct wordsplit *wsp, struct wordsplit_node *node)
{
  struct wordsplit_node *p, *end;
  size_t len = 0;
  char *buf, *cur;
  int stop;

  for (p = node; p && (p->flags & _WSNF_JOIN); p = p->next)
    {
      len += wsnode_len (p);
    }
  len += wsnode_len (p);
  end = p;

  buf = malloc (len + 1);
  if (!buf)
    return _wsplt_nomem (wsp);
  cur = buf;

  p = node;
  for (stop = 0; !stop;)
    {
      struct wordsplit_node *next = p->next;
      const char *str = wsnode_ptr (wsp, p);
      size_t slen = wsnode_len (p);

      memcpy (cur, str, slen);
      cur += slen;
      if (p != node)
	{
	  wsnode_remove (wsp, p);
	  stop = p == end;
	  wsnode_free (p);
	}
      p = next;
    }

  *cur = 0;

  node->flags &= ~_WSNF_JOIN;

  if (node->flags & _WSNF_WORD)
    free (node->v.word);
  else
    node->flags |= _WSNF_WORD;
  node->v.word = buf;
  return 0;
}

static int
wsnode_quoteremoval (struct wordsplit *wsp)
{
  struct wordsplit_node *p;
  void (*uqfn) (char *, const char *, size_t) =
    (wsp->ws_flags & WRDSF_CESCAPES) ?
    wordsplit_c_unquote_copy : wordsplit_sh_unquote_copy;

  for (p = wsp->ws_head; p; p = p->next)
    {
      const char *str = wsnode_ptr (wsp, p);
      size_t slen = wsnode_len (p);
      int unquote;

      if (wsp->ws_flags & WRDSF_QUOTE)
	{
	  unquote = !(p->flags & _WSNF_NOEXPAND);
	}
      else
	unquote = 0;

      if (unquote)
	{
	  if (!(p->flags & _WSNF_WORD))
	    {
	      char *newstr = malloc (slen + 1);
	      if (!newstr)
		return _wsplt_nomem (wsp);
	      memcpy (newstr, str, slen);
	      newstr[slen] = 0;
	      p->v.word = newstr;
	      p->flags |= _WSNF_WORD;
	    }

	  if (wsp->ws_flags & WRDSF_ESCAPE)
	    wordsplit_general_unquote_copy (p->v.word, str, slen,
				            wsp->ws_escape);
	  else
	    uqfn (p->v.word, str, slen);
	}
    }
  return 0;
}

static int
wsnode_coalesce (struct wordsplit *wsp)
{
  struct wordsplit_node *p;

  for (p = wsp->ws_head; p; p = p->next)
    {
      if (p->flags & _WSNF_JOIN)
	if (coalesce_segment (wsp, p))
	  return 1;
    }
  return 0;
}

static int
wordsplit_finish (struct wordsplit *wsp)
{
  struct wordsplit_node *p;
  size_t n;

  n = 0;

  for (p = wsp->ws_head; p; p = p->next)
    n++;

  if (alloc_space (wsp, n + 1))
    return 1;

  for (p = wsp->ws_head; p; p = p->next)
    {
      const char *str = wsnode_ptr (wsp, p);
      size_t slen = wsnode_len (p);
      char *newstr = malloc (slen + 1);

      /* Assign newstr first, even if it is NULL.  This way
         wordsplit_free will work even if we return
         nomem later. */
      wsp->ws_wordv[wsp->ws_offs + wsp->ws_wordc] = newstr;
      if (!newstr)
	return _wsplt_nomem (wsp);
      memcpy (newstr, str, slen);
      newstr[slen] = 0;

      wsp->ws_wordc++;

    }
  wsp->ws_wordv[wsp->ws_offs + wsp->ws_wordc] = NULL;
  return 0;
}


/* Variable expansion */
static int
node_split_prefix (struct wordsplit *wsp,
		   struct wordsplit_node **ptail,
		   struct wordsplit_node *node,
		   size_t beg, size_t len, int flg)
{
  struct wordsplit_node *newnode;

  if (len == 0)
    return 0;
  if (wsnode_new (wsp, &newnode))
    return 1;
  wsnode_insert (wsp, newnode, *ptail, 0);
  if (node->flags & _WSNF_WORD)
    {
      const char *str = wsnode_ptr (wsp, node);
      char *newstr = malloc (len + 1);
      if (!newstr)
	return _wsplt_nomem (wsp);
      memcpy (newstr, str + beg, len);
      newstr[len] = 0;
      newnode->flags = _WSNF_WORD;
      newnode->v.word = newstr;
    }
  else
    {
      newnode->v.segm.beg = node->v.segm.beg + beg;
      newnode->v.segm.end = newnode->v.segm.beg + len;
    }
  newnode->flags |= flg;
  *ptail = newnode;
  return 0;
}

static int
find_closing_cbrace (const char *str, size_t i, size_t len, size_t * poff)
{
  enum
  { st_init, st_squote, st_dquote } state = st_init;
  size_t level = 1;

  for (; i < len; i++)
    {
      switch (state)
	{
	case st_init:
	  switch (str[i])
	    {
	    case '{':
	      level++;
	      break;

	    case '}':
	      if (--level == 0)
		{
		  *poff = i;
		  return 0;
		}
	      break;

	    case '"':
	      state = st_dquote;
	      break;

	    case '\'':
	      state = st_squote;
	      break;
	    }
	  break;

	case st_squote:
	  if (str[i] == '\'')
	    state = st_init;
	  break;

	case st_dquote:
	  if (str[i] == '\\')
	    i++;
	  else if (str[i] == '"')
	    state = st_init;
	  break;
	}
    }
  return 1;
}

static const char *
wordsplit_find_env (struct wordsplit *wsp, const char *name, size_t len)
{
  size_t i;

  if (!(wsp->ws_flags & WRDSF_ENV))
    return NULL;

  if (wsp->ws_flags & WRDSF_ENV_KV)
    {
      /* A key-value pair environment */
      for (i = 0; wsp->ws_env[i]; i++)
	{
	  size_t elen = strlen (wsp->ws_env[i]);
	  if (elen == len && memcmp (wsp->ws_env[i], name, elen) == 0)
	    return wsp->ws_env[i + 1];
	  /* Skip the value.  Break the loop if it is NULL. */
	  i++;
	  if (wsp->ws_env[i] == NULL)
	    break;
	}
    }
  else
    {
      /* Usual (A=B) environment. */
      for (i = 0; wsp->ws_env[i]; i++)
	{
	  size_t j;
	  const char *var = wsp->ws_env[i];

	  for (j = 0; j < len; j++)
	    if (name[j] != var[j])
	      break;
	  if (j == len && var[j] == '=')
	    return var + j + 1;
	}
    }
  return NULL;
}

static int
expvar (struct wordsplit *wsp, const char *str, size_t len,
	struct wordsplit_node **ptail, const char **pend, int flg)
{
  size_t i = 0;
  const char *defstr = NULL;
  const char *value;
  const char *vptr;
  struct wordsplit_node *newnode;
  const char *start = str - 1;

  if (ISALPHA (str[0]) || str[0] == '_')
    {
      for (i = 1; i < len; i++)
	if (!(ISALNUM (str[i]) || str[i] == '_'))
	  break;
      *pend = str + i - 1;
    }
  else if (str[0] == '{')
    {
      str++;
      len--;
      for (i = 1; i < len; i++)
	if (str[i] == '}' || str[i] == ':')
	  break;
      if (str[i] == ':')
	{
	  size_t j;

	  defstr = str + i + 1;
	  if (find_closing_cbrace (str, i + 1, len, &j))
	    {
	      wsp->ws_errno = WRDSE_CBRACE;
	      return 1;
	    }
	  *pend = str + j;
	}
      else if (str[i] == '}')
	{
	  defstr = NULL;
	  *pend = str + i;
	}
      else
	{
	  wsp->ws_errno = WRDSE_CBRACE;
	  return 1;
	}
    }
  else
    {
      if (wsnode_new (wsp, &newnode))
	return 1;
      wsnode_insert (wsp, newnode, *ptail, 0);
      *ptail = newnode;
      newnode->flags = _WSNF_WORD | flg;
      newnode->v.word = malloc (3);
      if (!newnode->v.word)
	return _wsplt_nomem (wsp);
      newnode->v.word[0] = '$';
      newnode->v.word[1] = str[0];
      newnode->v.word[2] = 0;
      *pend = str;
      return 0;
    }

  /* Actually expand the variable */
  /* str - start of the variable name
     i   - its length
     defstr - default replacement str */

  vptr = wordsplit_find_env (wsp, str, i);
  if (vptr)
    {
      value = strdup (vptr);
      if (!value)
	return _wsplt_nomem (wsp);
    }
  else if (wsp->ws_flags & WRDSF_GETVAR)
    value = wsp->ws_getvar (str, i, wsp->ws_closure);
  else if (wsp->ws_flags & WRDSF_UNDEF)
    {
      wsp->ws_errno = WRDSE_UNDEF;
      if (wsp->ws_flags & WRDSF_SHOWERR)
	wordsplit_perror (wsp);
      return 1;
    }
  else
    {
      if (wsp->ws_flags & WRDSF_WARNUNDEF)
	wsp->ws_error (_("warning: undefined variable `%.*s'"), (int) i, str);
      if (wsp->ws_flags & WRDSF_KEEPUNDEF)
	value = NULL;
      else
	value = "";
    }

  /* FIXME: handle defstr */
  (void) defstr;

  if (value)
    {
      if (flg & _WSNF_QUOTE)
	{
	  if (wsnode_new (wsp, &newnode))
	    return 1;
	  wsnode_insert (wsp, newnode, *ptail, 0);
	  *ptail = newnode;
	  newnode->flags = _WSNF_WORD | _WSNF_NOEXPAND | flg;
	  newnode->v.word = strdup (value);
	  if (!newnode->v.word)
	    return _wsplt_nomem (wsp);
	}
      else if (*value == 0)
	{
	  /* Empty string is a special case */
	  if (wsnode_new (wsp, &newnode))
	    return 1;
	  wsnode_insert (wsp, newnode, *ptail, 0);
	  *ptail = newnode;
	  newnode->flags = _WSNF_NULL;
	}
      else
	{
	  struct wordsplit ws;
	  int i;

	  ws.ws_delim = wsp->ws_delim;
	  if (wordsplit (value, &ws,
			 WRDSF_NOVAR | WRDSF_NOCMD | WRDSF_DELIM | WRDSF_WS))
	    {
	      wordsplit_free (&ws);
	      return 1;
	    }
	  for (i = 0; i < ws.ws_wordc; i++)
	    {
	      if (wsnode_new (wsp, &newnode))
		return 1;
	      wsnode_insert (wsp, newnode, *ptail, 0);
	      *ptail = newnode;
	      newnode->flags = _WSNF_WORD |
		_WSNF_NOEXPAND |
		(i + 1 < ws.ws_wordc ? (flg & ~_WSNF_JOIN) : flg);
	      newnode->v.word = strdup (ws.ws_wordv[i]);
	      if (!newnode->v.word)
		return _wsplt_nomem (wsp);
	    }
	  wordsplit_free (&ws);
	}
    }
  else if (wsp->ws_flags & WRDSF_KEEPUNDEF)
    {
      size_t size = *pend - start + 1;

      if (wsnode_new (wsp, &newnode))
	return 1;
      wsnode_insert (wsp, newnode, *ptail, 0);
      *ptail = newnode;
      newnode->flags = _WSNF_WORD | _WSNF_NOEXPAND | flg;
      newnode->v.word = malloc (size + 1);
      if (!newnode->v.word)
	return _wsplt_nomem (wsp);
      memcpy (newnode->v.word, start, size);
      newnode->v.word[size] = 0;
    }
  else
    {
      if (wsnode_new (wsp, &newnode))
	return 1;
      wsnode_insert (wsp, newnode, *ptail, 0);
      *ptail = newnode;
      newnode->flags = _WSNF_NULL;
    }
  return 0;
}

static int
node_expand_vars (struct wordsplit *wsp, struct wordsplit_node *node)
{
  const char *str = wsnode_ptr (wsp, node);
  size_t slen = wsnode_len (node);
  const char *end = str + slen;
  const char *p;
  size_t off = 0;
  struct wordsplit_node *tail = node;

  for (p = str; p < end; p++)
    {
      if (*p == '\\')
	{
	  p++;
	  continue;
	}
      if (*p == '$')
	{
	  size_t n = p - str;

	  if (tail != node)
	    tail->flags |= _WSNF_JOIN;
	  if (node_split_prefix (wsp, &tail, node, off, n, _WSNF_JOIN))
	    return 1;
	  p++;
	  if (expvar (wsp, p, slen - n, &tail, &p,
		      node->flags & (_WSNF_JOIN | _WSNF_QUOTE)))
	    return 1;
	  off += p - str + 1;
	  str = p + 1;
	}
    }
  if (p > str)
    {
      if (tail != node)
	tail->flags |= _WSNF_JOIN;
      if (node_split_prefix (wsp, &tail, node, off, p - str,
			     node->flags & _WSNF_JOIN))
	return 1;
    }
  if (tail != node)
    {
      wsnode_remove (wsp, node);
      wsnode_free (node);
    }
  return 0;
}

/* Remove NULL lists */
static void
wsnode_nullelim (struct wordsplit *wsp)
{
  struct wordsplit_node *p;

  for (p = wsp->ws_head; p;)
    {
      struct wordsplit_node *next = p->next;
      if (p->flags & _WSNF_NULL)
	{
	  wsnode_remove (wsp, p);
	  wsnode_free (p);
	}
      p = next;
    }
}

static int
wordsplit_varexp (struct wordsplit *wsp)
{
  struct wordsplit_node *p;

  for (p = wsp->ws_head; p;)
    {
      struct wordsplit_node *next = p->next;
      if (!(p->flags & _WSNF_NOEXPAND))
	if (node_expand_vars (wsp, p))
	  return 1;
      p = next;
    }

  wsnode_nullelim (wsp);
  return 0;
}

/* Strip off any leading and trailing whitespace.  This function is called
   right after the initial scanning, therefore it assumes that every
   node in the list is a text reference node. */
static void
wordsplit_trimws (struct wordsplit *wsp)
{
  struct wordsplit_node *p;

  for (p = wsp->ws_head; p; p = p->next)
    {
      size_t n;

      if (p->flags & _WSNF_QUOTE)
	continue;

      /* Skip leading whitespace: */
      for (n = p->v.segm.beg; n < p->v.segm.end && ISWS (wsp->ws_input[n]);
	   n++)
	;
      p->v.segm.beg = n;
      /* Trim trailing whitespace */
      for (n = p->v.segm.end;
	   n > p->v.segm.beg && ISWS (wsp->ws_input[n - 1]); n--);
      p->v.segm.end = n;
      if (p->v.segm.beg == p->v.segm.end)
	p->flags |= _WSNF_NULL;
    }

  wsnode_nullelim (wsp);
}

static int
skip_sed_expr (const char *command, size_t i, size_t len)
{
  int state;

  do
    {
      int delim;

      if (command[i] == ';')
	i++;
      if (!(command[i] == 's' && i + 3 < len && ISPUNCT (command[i + 1])))
	break;

      delim = command[++i];
      state = 1;
      for (i++; i < len; i++)
	{
	  if (state == 3)
	    {
	      if (command[i] == delim || !ISALNUM (command[i]))
		break;
	    }
	  else if (command[i] == '\\')
	    i++;
	  else if (command[i] == delim)
	    state++;
	}
    }
  while (state == 3 && i < len && command[i] == ';');
  return i;
}

static size_t
skip_delim (struct wordsplit *wsp)
{
  size_t start = wsp->ws_endp;
  if (wsp->ws_flags & WRDSF_SQUEEZE_DELIMS)
    {
      if ((wsp->ws_flags & WRDSF_RETURN_DELIMS) &&
	  ISDELIM (wsp, wsp->ws_input[start]))
	{
	  int delim = wsp->ws_input[start];
	  do
	    start++;
	  while (start < wsp->ws_len && delim == wsp->ws_input[start]);
	}
      else
	{
	  do
	    start++;
	  while (start < wsp->ws_len && ISDELIM (wsp, wsp->ws_input[start]));
	}
      start--;
    }

  if (!(wsp->ws_flags & WRDSF_RETURN_DELIMS))
    start++;

  return start;
}

#define _WRDS_EOF   0
#define _WRDS_OK    1
#define _WRDS_ERR   2

static int
scan_qstring (struct wordsplit *wsp, size_t start, size_t * end)
{
  size_t j;
  const char *command = wsp->ws_input;
  size_t len = wsp->ws_len;
  char q = command[start];

  for (j = start + 1; j < len && command[j] != q; j++)
    if (q == '"' && command[j] == '\\')
      j++;
  if (j < len && command[j] == q)
    {
      int flags = _WSNF_QUOTE | _WSNF_EMPTYOK;
      if (q == '\'')
	flags |= _WSNF_NOEXPAND;
      if (wordsplit_add_segm (wsp, start + 1, j, flags))
	return _WRDS_ERR;
      *end = j;
    }
  else
    {
      wsp->ws_endp = start;
      wsp->ws_errno = WRDSE_QUOTE;
      if (wsp->ws_flags & WRDSF_SHOWERR)
	wordsplit_perror (wsp);
      return _WRDS_ERR;
    }
  return 0;
}

static int
scan_word (struct wordsplit *wsp, size_t start)
{
  size_t len = wsp->ws_len;
  const char *command = wsp->ws_input;
  const char *comment = wsp->ws_comment;
  int join = 0;
  int flags = 0;

  size_t i = start;

  if (i >= len)
    {
      wsp->ws_errno = WRDSE_EOF;
      return _WRDS_EOF;
    }

  start = i;

  if (wsp->ws_flags & WRDSF_SED_EXPR
      && command[i] == 's' && i + 3 < len && ISPUNCT (command[i + 1]))
    {
      flags = _WSNF_SEXP;
      i = skip_sed_expr (command, i, len);
    }
  else if (!ISDELIM (wsp, command[i]))
    {
      while (i < len)
	{
	  if (comment && strchr (comment, command[i]) != NULL)
	    {
	      size_t j;
	      for (j = i + 1; j < len && command[j] != '\n'; j++)
		;
	      if (wordsplit_add_segm (wsp, start, i, 0))
		return _WRDS_ERR;
	      wsp->ws_endp = j;
	      return _WRDS_OK;
	    }

	  if (wsp->ws_flags & WRDSF_QUOTE)
	    {
	      if (command[i] == '\\')
		{
		  if (++i == len)
		    break;
		  i++;
		  continue;
		}

	      if (((wsp->ws_flags & WRDSF_SQUOTE) && command[i] == '\'') ||
		  ((wsp->ws_flags & WRDSF_DQUOTE) && command[i] == '"'))
		{
		  if (join && wsp->ws_tail)
		    wsp->ws_tail->flags |= _WSNF_JOIN;
		  if (wordsplit_add_segm (wsp, start, i, _WSNF_JOIN))
		    return _WRDS_ERR;
		  if (scan_qstring (wsp, i, &i))
		    return _WRDS_ERR;
		  start = i + 1;
		  join = 1;
		}
	    }

	  if (ISDELIM (wsp, command[i]))
	    break;
	  else
	    i++;
	}
    }
  else if (wsp->ws_flags & WRDSF_RETURN_DELIMS)
    {
      i++;
    }
  else if (!(wsp->ws_flags & WRDSF_SQUEEZE_DELIMS))
    flags |= _WSNF_EMPTYOK;

  if (join && i > start && wsp->ws_tail)
    wsp->ws_tail->flags |= _WSNF_JOIN;
  if (wordsplit_add_segm (wsp, start, i, flags))
    return _WRDS_ERR;
  wsp->ws_endp = i;
  if (wsp->ws_flags & WRDSF_INCREMENTAL)
    return _WRDS_EOF;
  return _WRDS_OK;
}

static char quote_transtab[] = "\\\\\"\"a\ab\bf\fn\nr\rt\tv\v";

int
wordsplit_c_unquote_char (int c)
{
  char *p;

  for (p = quote_transtab; *p; p += 2)
    {
      if (*p == c)
	return p[1];
    }
  return c;
}

int
wordsplit_c_quote_char (int c)
{
  char *p;

  for (p = quote_transtab + sizeof (quote_transtab) - 2;
       p > quote_transtab; p -= 2)
    {
      if (*p == c)
	return p[-1];
    }
  return -1;
}

#define to_num(c) \
  (ISDIGIT(c) ? c - '0' : (ISXDIGIT(c) ? toupper(c) - 'A' + 10 : 255 ))

static int
xtonum (int *pval, const char *src, int base, int cnt)
{
  int i, val;

  for (i = 0, val = 0; i < cnt; i++, src++)
    {
      int n = *(unsigned char *) src;
      if (n > 127 || (n = to_num (n)) >= base)
	break;
      val = val * base + n;
    }
  *pval = val;
  return i;
}

size_t
wordsplit_c_quoted_length (const char *str, int quote_hex, int *quote)
{
  size_t len = 0;

  *quote = 0;
  for (; *str; str++)
    {
      if (strchr (" \"", *str))
	*quote = 1;

      if (*str == ' ')
	len++;
      else if (*str == '"')
	len += 2;
      else if (*str != '\t' && *str != '\\' && ISPRINT (*str))
	len++;
      else if (quote_hex)
	len += 3;
      else
	{
	  if (wordsplit_c_quote_char (*str) != -1)
	    len += 2;
	  else
	    len += 4;
	}
    }
  return len;
}

void
wordsplit_general_unquote_copy (char *dst, const char *src, size_t n,
				   const char *escapable)
{
  int i;

  for (i = 0; i < n;)
    {
      if (src[i] == '\\' && i < n && strchr (escapable, src[i + 1]))
	i++;
      *dst++ = src[i++];
    }
  *dst = 0;
}

void
wordsplit_sh_unquote_copy (char *dst, const char *src, size_t n)
{
  int i;

  for (i = 0; i < n;)
    {
      if (src[i] == '\\')
	i++;
      *dst++ = src[i++];
    }
  *dst = 0;
}

void
wordsplit_c_unquote_copy (char *dst, const char *src, size_t n)
{
  int i = 0;
  int c;

  while (i < n)
    {
      if (src[i] == '\\')
	{
	  ++i;
	  if (src[i] == 'x' || src[i] == 'X')
	    {
	      if (n - i < 2)
		{
		  *dst++ = '\\';
		  *dst++ = src[i++];
		}
	      else
		{
		  int off = xtonum (&c, src + i + 1,
				    16, 2);
		  if (off == 0)
		    {
		      *dst++ = '\\';
		      *dst++ = src[i++];
		    }
		  else
		    {
		      *dst++ = c;
		      i += off + 1;
		    }
		}
	    }
	  else if ((unsigned char) src[i] < 128 && ISDIGIT (src[i]))
	    {
	      if (n - i < 1)
		{
		  *dst++ = '\\';
		  *dst++ = src[i++];
		}
	      else
		{
		  int off = xtonum (&c, src + i, 8, 3);
		  if (off == 0)
		    {
		      *dst++ = '\\';
		      *dst++ = src[i++];
		    }
		  else
		    {
		      *dst++ = c;
		      i += off;
		    }
		}
	    }
	  else
	    *dst++ = wordsplit_c_unquote_char (src[i++]);
	}
      else
	*dst++ = src[i++];
    }
  *dst = 0;
}

void
wordsplit_c_quote_copy (char *dst, const char *src, int quote_hex)
{
  for (; *src; src++)
    {
      if (*src == '"')
	{
	  *dst++ = '\\';
	  *dst++ = *src;
	}
      else if (*src != '\t' && *src != '\\' && ISPRINT (*src))
	*dst++ = *src;
      else
	{
	  char tmp[4];

	  if (quote_hex)
	    {
	      snprintf (tmp, sizeof tmp, "%%%02X", *(unsigned char *) src);
	      memcpy (dst, tmp, 3);
	      dst += 3;
	    }
	  else
	    {
	      int c = wordsplit_c_quote_char (*src);
	      *dst++ = '\\';
	      if (c != -1)
		*dst++ = c;
	      else
		{
		  snprintf (tmp, sizeof tmp, "%03o", *(unsigned char *) src);
		  memcpy (dst, tmp, 3);
		  dst += 3;
		}
	    }
	}
    }
}

static int
wordsplit_process_list (struct wordsplit *wsp, size_t start)
{
  if (wsp->ws_flags & WRDSF_NOSPLIT)
    {
      /* Treat entire input as a quoted argument */
      if (wordsplit_add_segm (wsp, start, wsp->ws_len, _WSNF_QUOTE))
	return wsp->ws_errno;
    }
  else
    {
      int rc;

      while ((rc = scan_word (wsp, start)) == _WRDS_OK)
	start = skip_delim (wsp);
      /* Make sure tail element is not joinable */
      if (wsp->ws_tail)
	wsp->ws_tail->flags &= ~_WSNF_JOIN;
      if (rc == _WRDS_ERR)
	return wsp->ws_errno;
    }

  if (wsp->ws_flags & WRDSF_SHOWDBG)
    {
      wsp->ws_debug ("Initial list:");
      wordsplit_dump_nodes (wsp);
    }

  if (wsp->ws_flags & WRDSF_WS)
    {
      /* Trim leading and trailing whitespace */
      wordsplit_trimws (wsp);
      if (wsp->ws_flags & WRDSF_SHOWDBG)
	{
	  wsp->ws_debug ("After WS trimming:");
	  wordsplit_dump_nodes (wsp);
	}
    }

  /* Expand variables (FIXME: & commands) */
  if (!(wsp->ws_flags & WRDSF_NOVAR))
    {
      if (wordsplit_varexp (wsp))
	{
	  wordsplit_free_nodes (wsp);
	  return wsp->ws_errno;
	}
      if (wsp->ws_flags & WRDSF_SHOWDBG)
	{
	  wsp->ws_debug ("Expanded list:");
	  wordsplit_dump_nodes (wsp);
	}
    }

  do
    {
      if (wsnode_quoteremoval (wsp))
	break;
      if (wsp->ws_flags & WRDSF_SHOWDBG)
	{
	  wsp->ws_debug ("After quote removal:");
	  wordsplit_dump_nodes (wsp);
	}

      if (wsnode_coalesce (wsp))
	break;

      if (wsp->ws_flags & WRDSF_SHOWDBG)
	{
	  wsp->ws_debug ("Coalesced list:");
	  wordsplit_dump_nodes (wsp);
	}
    }
  while (0);
  return wsp->ws_errno;
}

int
wordsplit_len (const char *command, size_t length, struct wordsplit *wsp,
               int flags)
{
  int rc;
  size_t start;
  const char *cmdptr;
  size_t cmdlen;

  if (!command)
    {
      if (!(flags & WRDSF_INCREMENTAL))
	return EINVAL;

      start = skip_delim (wsp);
      if (wsp->ws_endp == wsp->ws_len)
	{
	  wsp->ws_errno = WRDSE_NOINPUT;
	  if (wsp->ws_flags & WRDSF_SHOWERR)
	    wordsplit_perror (wsp);
	  return wsp->ws_errno;
	}

      cmdptr = wsp->ws_input + wsp->ws_endp;
      cmdlen = wsp->ws_len - wsp->ws_endp;
      wsp->ws_flags |= WRDSF_REUSE;
      wordsplit_init0 (wsp);
    }
  else
    {
      cmdptr = command;
      cmdlen = length;
      start = 0;
      rc = wordsplit_init (wsp, cmdptr, cmdlen, flags);
      if (rc)
	return rc;
    }

  if (wsp->ws_flags & WRDSF_SHOWDBG)
    wsp->ws_debug ("Input:%.*s;", (int) cmdlen, cmdptr);

  rc = wordsplit_process_list (wsp, start);
  if (rc == 0 && (flags & WRDSF_INCREMENTAL))
    {
      while (!wsp->ws_head && wsp->ws_endp < wsp->ws_len)
	{
	  start = skip_delim (wsp);
	  if (wsp->ws_flags & WRDSF_SHOWDBG)
	    {
	      cmdptr = wsp->ws_input + wsp->ws_endp;
	      cmdlen = wsp->ws_len - wsp->ws_endp;
	      wsp->ws_debug ("Restart:%.*s;", (int) cmdlen, cmdptr);
	    }
	  rc = wordsplit_process_list (wsp, start);
	  if (rc)
	    break;
	}
    }
  if (rc)
    {
      wordsplit_free_nodes (wsp);
      return rc;
    }
  wordsplit_finish (wsp);
  wordsplit_free_nodes (wsp);
  return wsp->ws_errno;
}

int
wordsplit (const char *command, struct wordsplit *ws, int flags)
{
  return wordsplit_len (command, command ? strlen (command) : 0, ws,
			   flags);
}

void
wordsplit_free_words (struct wordsplit *ws)
{
  size_t i;

  for (i = 0; i < ws->ws_wordc; i++)
    {
      char *p = ws->ws_wordv[ws->ws_offs + i];
      if (p)
	{
	  free (p);
	  ws->ws_wordv[ws->ws_offs + i] = NULL;
	}
    }
  ws->ws_wordc = 0;
}

void
wordsplit_free (struct wordsplit *ws)
{
  wordsplit_free_words (ws);
  free (ws->ws_wordv);
  ws->ws_wordv = NULL;
}

void
wordsplit_perror (struct wordsplit *wsp)
{
  switch (wsp->ws_errno)
    {
    case WRDSE_EOF:
      wsp->ws_error (_("no error"));
      break;

    case WRDSE_QUOTE:
      wsp->ws_error (_("missing closing %c (start near #%lu)"),
		     wsp->ws_input[wsp->ws_endp],
		     (unsigned long) wsp->ws_endp);
      break;

    case WRDSE_NOSPACE:
      wsp->ws_error (_("memory exhausted"));
      break;

    case WRDSE_NOSUPP:
      wsp->ws_error (_("command substitution is not yet supported"));

    case WRDSE_USAGE:
      wsp->ws_error (_("invalid wordsplit usage"));
      break;

    case WRDSE_CBRACE:
      wsp->ws_error (_("unbalanced curly brace"));
      break;

    case WRDSE_UNDEF:
      wsp->ws_error (_("undefined variable"));
      break;

    case WRDSE_NOINPUT:
      wsp->ws_error (_("input exhausted"));
      break;

    default:
      wsp->ws_error (_("unknown error"));
    }
}

const char *_wordsplit_errstr[] = {
  N_("no error"),
  N_("missing closing quote"),
  N_("memory exhausted"),
  N_("command substitution is not yet supported"),
  N_("invalid wordsplit usage"),
  N_("unbalanced curly brace"),
  N_("undefined variable"),
  N_("input exhausted")
};
int _wordsplit_nerrs =
  sizeof (_wordsplit_errstr) / sizeof (_wordsplit_errstr[0]);

const char *
wordsplit_strerror (struct wordsplit *ws)
{
  if (ws->ws_errno < _wordsplit_nerrs)
    return _wordsplit_errstr[ws->ws_errno];
  return N_("unknown error");
}
