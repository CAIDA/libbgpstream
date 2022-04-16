/*
 * Copyright (C) 2016 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Shane Alcock <salcock@waikato.ac.nz>
 */

#include "bgpstream_filter_parser.h"
#include "bgpstream_filter.h"
#include "bgpstream_log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *bgpstream_filter_type_to_string(bgpstream_filter_type_t type)
{
  switch (type) {
  case BGPSTREAM_FILTER_TYPE_RECORD_TYPE:
    return "Record Type";
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE:
    return "Prefix (or more specific)";
  case BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY:
    return "Community";
  case BGPSTREAM_FILTER_TYPE_ELEM_NOT_PEER_ASN:
    return "Not Peer ASN";
  case BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN:
    return "Peer ASN";
  case BGPSTREAM_FILTER_TYPE_ELEM_ORIGIN_ASN:
    return "Origin ASN";
  case BGPSTREAM_FILTER_TYPE_PROJECT:
    return "Project";
  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
    return "Collector";
  case BGPSTREAM_FILTER_TYPE_ROUTER:
    return "Router";
  case BGPSTREAM_FILTER_TYPE_ELEM_ASPATH:
    return "AS Path";
  case BGPSTREAM_FILTER_TYPE_ELEM_EXTENDED_COMMUNITY:
    return "Extended Community";
  case BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION:
    return "IP Version";
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY:
    return "Prefix (of any specificity)";
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS:
    return "Prefix (or less specific)";
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT:
    return "Prefix (exact match)";
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX:
    return "Prefix (old format)";
  case BGPSTREAM_FILTER_TYPE_ELEM_TYPE:
    return "Element Type";
  case BGPSTREAM_FILTER_TYPE_RESOURCE_TYPE:
    return "Resource Type";
  }

  return "Unknown filter term ??";
}

static int instantiate_filter(bgpstream_t *bs, bgpstream_filter_item_t *item)
{
  bgpstream_filter_type_t usetype = item->termtype;

  switch (item->termtype) {
  case BGPSTREAM_FILTER_TYPE_RECORD_TYPE:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT:
  case BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY:
  case BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN:
  case BGPSTREAM_FILTER_TYPE_ELEM_NOT_PEER_ASN:
  case BGPSTREAM_FILTER_TYPE_ELEM_ORIGIN_ASN:
  case BGPSTREAM_FILTER_TYPE_PROJECT:
  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
  case BGPSTREAM_FILTER_TYPE_ROUTER:
  case BGPSTREAM_FILTER_TYPE_ELEM_ASPATH:
  case BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION:
  case BGPSTREAM_FILTER_TYPE_ELEM_TYPE:
  case BGPSTREAM_FILTER_TYPE_RESOURCE_TYPE:
    bgpstream_log(BGPSTREAM_LOG_FINE, "Adding filter: %s '%s'",
        bgpstream_filter_type_to_string(item->termtype), item->value);
    if (!bgpstream_add_filter(bs, usetype, item->value))
      return 0;
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Implementation of filter type %s is still to come!",
                  bgpstream_filter_type_to_string(item->termtype));
    return 0;
  }
  return 1;
}

// List of terms.
// repeatable:
//   >= 0: term may appear this many more times, and takes a list of values
//   < 0: term may be repeated any number of times, but takes a single value
#define TERMS(X) \
  /* repeatable, word, alt, termtype, state */ \
  X(1,  "project",      "proj", PROJECT,                 VALUE) \
  X(1,  "collector",    "coll", COLLECTOR,               VALUE) \
  X(1,  "router",       "rout", ROUTER,                  VALUE) \
  X(1,  "type",         NULL,   RECORD_TYPE,             VALUE) \
  X(1,  "resourcetype", "restype", RESOURCE_TYPE,        VALUE)  \
  X(1,  "peer",         NULL,   ELEM_PEER_ASN,           VALUE) \
  X(1,  "origin",       "orig", ELEM_ORIGIN_ASN,         VALUE)     \
  X(1,  "prefix",       "pref", ELEM_PREFIX_MORE,        PREFIXEXT) \
                                /* ^^^ XXX is MORE the best default? */ \
  X(1,  "community",    "comm", ELEM_COMMUNITY,          VALUE) \
  X(-1, "aspath",       "path", ELEM_ASPATH,             VALUE) \
  X(1,  "extcommunity", "extc", ELEM_EXTENDED_COMMUNITY, VALUE) \
  X(1,  "ipversion",    "ipv",  ELEM_IP_VERSION,         VALUE) \
  X(1,  "elemtype",     NULL,   ELEM_TYPE,               VALUE) \
  /* for state transition in bgpstream_parse_endvalue() */ \
  X(0,  "prefix",       NULL,   ELEM_PREFIX_ANY,         PREFIXEXT) \
  X(0,  "prefix",       NULL,   ELEM_PREFIX_MORE,        PREFIXEXT) \
  X(0,  "prefix",       NULL,   ELEM_PREFIX_LESS,        PREFIXEXT) \
  X(0,  "prefix",       NULL,   ELEM_PREFIX_EXACT,       PREFIXEXT)


static const struct {
  const char *word; // full name
  const char *alt; // alternate spelling
  bgpstream_filter_type_t termtype;
  fp_state_t state;
} terms[] = {
  #define TERM_INITIALIZER(repeatable, word, alt, termtype, state) \
    { (word), (alt), BGPSTREAM_FILTER_TYPE_##termtype, (state) },
  TERMS(TERM_INITIALIZER)
  { NULL, NULL, 0, 0 }
};

static fp_state_t bgpstream_parse_filter_term(const char *term, size_t len,
    fp_state_t *state, bgpstream_filter_item_t *curr, int *repeatable)
{

  for (int i = 0; terms[i].word; ++i) {
    if ((strncmp(term, terms[i].word, len) == 0 && terms[i].word[len] == '\0') ||
      (terms[i].alt &&
       strncmp(term, terms[i].alt, len) == 0 && terms[i].alt[len] == '\0'))
    {
      if (repeatable[i] == 0) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Term '%*s' used more than once",
          (int)len, terms[i].word);
        return *state = FAIL;
      } else if (repeatable[i] > 0) {
        repeatable[i]--;
      }
      bgpstream_log(BGPSTREAM_LOG_FINE, "term '%s', state %d",
          terms[i].word, terms[i].state);
      curr->termtype = terms[i].termtype;
      return *state = terms[i].state;
    }
  }

  bgpstream_log(BGPSTREAM_LOG_ERR, "Expected a valid term, found '%.*s'",
    (int)len, term);
  return *state = FAIL;
}

static int bgpstream_parse_value(const char *value, size_t *lenp,
    fp_state_t *state, bgpstream_filter_item_t *curr)
{
  if (curr->value) {
    free(curr->value);
    curr->value = NULL;
  }

  if (*value == '"') {
    // quoted string value
    size_t len = strcspn(value + 1, "\"");
    if (value[1+len] != '"') {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Missing closing quote: '%s'", value);
      return *state = FAIL;
    }
    if (value[2+len] != ' ' && value[2+len] != '\0') {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Found garbage after quoted \"%.*s\"",
          (int)len, value+1);
      return *state = FAIL;
    }
    curr->value = strndup(value + 1, len);
    *lenp = len + 2; // string plus 2 quotes
  } else {
    // unquoted single-word value
    curr->value = strndup(value, *lenp);
  }

  /* XXX How intelligent do we want to be in terms of validating input? */

  /* At this point we can probably create our new filter */
  /* XXX this may not be true once we get around to OR support... */
  bgpstream_log(BGPSTREAM_LOG_FINE, "value: '%s'", curr->value);

  return *state = ENDVALUE;
}

static int bgpstream_parse_prefixext(const char *ext, size_t *lenp,
    fp_state_t *state, bgpstream_filter_item_t *curr)
{
  struct {
    const char *word;
    bgpstream_filter_type_t termtype;
  } kw[] = {
    /* Any prefix that our prefix belongs to */
    { "any",   BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY },
    /* Either match this prefix or any more specific prefixes */
    { "more",  BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE },
    /* Either match this prefix or any less specific prefixes */
    { "less",  BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS },
    /* Only match exactly this prefix */
    { "exact", BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT },
    { NULL, 0 }
  };

  size_t len = *lenp;
  for (int i = 0; kw[i].word; ++i) {
    if ((strncmp(ext, kw[i].word, len) == 0 && kw[i].word[len] == '\0')) {
      bgpstream_log(BGPSTREAM_LOG_FINE, "Got a '%s' prefix", kw[i].word);
      curr->termtype = kw[i].termtype;
      return *state = VALUE;
    }
  }

  /* At this point, assume we're looking at a value instead */
  return bgpstream_parse_value(ext, lenp, state, curr);
}

static fp_state_t bgpstream_parse_endvalue(const char *conj, size_t len,
    fp_state_t *state, bgpstream_filter_item_t *curr, int *repeatable)
{
  // We've already parsed TERM VALUE; now we expect "and" or another VALUE.
  if (strncmp(conj, "and", len) == 0 && len == 3) {
    return *state = TERM;
  } else if (strncmp(conj, "or", len) == 0 && len == 2) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "'or' is not yet implemented");
    return *state = FAIL;
  } else {
    for (int i = 0; terms[i].word; ++i) {
      if (curr->termtype == terms[i].termtype) {
        if (repeatable[i] < 0) {
          // this term doesn't allow a list of values
          bgpstream_log(BGPSTREAM_LOG_ERR,
            "term '%s' does not allow multiple values", terms[i].word);
          return *state = FAIL;
        }
        bgpstream_log(BGPSTREAM_LOG_FINE, "repeat term '%s', state %d",
            terms[i].word, terms[i].state);
        return *state = terms[i].state;
      }
    }
    return *state = FAIL;
  }
}

int bgpstream_parse_filter_string(bgpstream_t *bs, const char *fstring)
{
  int repeatable[] = {
    #define TERM_REPEATABLE(repeatable, word, alt, termtype, state)   (repeatable),
    TERMS(TERM_REPEATABLE)
  };
  const char *p;
  size_t len;
  int success = 0; // fail, until proven otherwise

  bgpstream_log(BGPSTREAM_LOG_FINE, "Parsing filter string: '%s'", fstring);
  bgpstream_filter_item_t *filteritem;
  fp_state_t state = TERM;

  filteritem =
    (bgpstream_filter_item_t *)calloc(1, sizeof(bgpstream_filter_item_t));

  for (p = fstring; ; p += len) {
    while (isspace(*p)) ++p;
    // Calculate length of next space-delimited token.  (Sub-parser will modify
    // this if the next token is not actually space-delmited.)
    len = strcspn(p, " ");
    if (len == 0) break;

retry_token:
    switch (state) {
    case TERM:
      if (bgpstream_parse_filter_term(p, len, &state, filteritem, repeatable) == FAIL) {
        goto endparsing;
      }
      break;

    case PREFIXEXT:
      if (bgpstream_parse_prefixext(p, &len, &state, filteritem) == FAIL) {
        goto endparsing;
      }
      if (state == ENDVALUE) {
        if (!instantiate_filter(bs, filteritem))
          goto endparsing;
      }
      break;

    case VALUE:
      if (bgpstream_parse_value(p, &len, &state, filteritem) == FAIL) {
        goto endparsing;
      }
      if (!instantiate_filter(bs, filteritem))
        goto endparsing;
      break;

    case ENDVALUE:
      if (bgpstream_parse_endvalue(p, len, &state, filteritem, repeatable) == FAIL) {
        goto endparsing;
      } else if (state == TERM) {
        continue; // got an "and"; continue with the next term
      } else {
        goto retry_token; // token was not "and"; retry it with a new state
      }

    default:
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Unexpected BGPStream filter string state: %d", state);
      goto endparsing;
    }
  }
  if (state == ENDVALUE) {
    bgpstream_log(BGPSTREAM_LOG_FINE, "Finished parsing filter string");
    success = 1; // success!
  } else {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Expected %s, found end of string",
        state == TERM ? "term" : "argument");
  }

endparsing:

  if (filteritem) {
    if (filteritem->value) {
      free(filteritem->value);
    }
    free(filteritem);
  }

  return success;
}
