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
  }

  return "Unknown filter term ??";
}

static void instantiate_filter(bgpstream_t *bs, bgpstream_filter_item_t *item)
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
  case BGPSTREAM_FILTER_TYPE_PROJECT:
  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
  case BGPSTREAM_FILTER_TYPE_ROUTER:
  case BGPSTREAM_FILTER_TYPE_ELEM_ASPATH:
  case BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION:
  case BGPSTREAM_FILTER_TYPE_ELEM_TYPE:
    bgpstream_log(BGPSTREAM_LOG_FINE, "Adding filter: %s '%s'",
        bgpstream_filter_type_to_string(item->termtype), item->value);
    bgpstream_add_filter(bs, usetype, item->value);
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Implementation of filter type %s is still to come!",
                  bgpstream_filter_type_to_string(item->termtype));
    break;
  }
}


static int bgpstream_parse_filter_term(const char *term, size_t len,
    fp_state_t *state, bgpstream_filter_item_t *curr)
{

  struct {
    const char *word; // full name
    const char *alt; // alternate spelling
    bgpstream_filter_type_t termtype;
    fp_state_t state;
  } kw[] = {
    { "project", "proj",
      BGPSTREAM_FILTER_TYPE_PROJECT, VALUE },
    { "collector", "coll",
      BGPSTREAM_FILTER_TYPE_COLLECTOR, VALUE },
    { "router", "rout",
      BGPSTREAM_FILTER_TYPE_ROUTER, VALUE },
    { "type", NULL,
      BGPSTREAM_FILTER_TYPE_RECORD_TYPE, VALUE },
    { "peer", NULL,
      BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN, VALUE },
    { "prefix", "pref",
      BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE, // XXX is this the best default?
      PREFIXEXT },
    { "community", "comm",
      BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY, VALUE },
    { "aspath", "path",
      BGPSTREAM_FILTER_TYPE_ELEM_ASPATH, VALUE },
    { "extcommunity", "extc",
      BGPSTREAM_FILTER_TYPE_ELEM_EXTENDED_COMMUNITY, VALUE },
    { "ipversion", "ipv",
      BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION, VALUE },
    { "elemtype", NULL,
      BGPSTREAM_FILTER_TYPE_ELEM_TYPE, VALUE },
    { NULL, NULL, 0, 0 }
  };

  for (int i = 0; kw[i].word; ++i) {
    if ((strncmp(term, kw[i].word, len) == 0 && kw[i].word[len] == '\0') ||
      (kw[i].alt &&
       strncmp(term, kw[i].alt, len) == 0 && kw[i].alt[len] == '\0'))
    {
      bgpstream_log(BGPSTREAM_LOG_FINE, "term: '%s'", kw[i].word);
      curr->termtype = kw[i].termtype;
      return *state = kw[i].state;
    }
  }

  bgpstream_log(BGPSTREAM_LOG_ERR, "Expected a valid term, found '%*s'",
    (int)len, term);
  return *state = FAIL;
}

static int bgpstream_parse_value(const char *value, size_t *lenp,
    fp_state_t *state, bgpstream_filter_item_t *curr)
{

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

  assert(curr->termtype == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE);

  struct {
    const char *word;
    bgpstream_filter_type_t termtype;
    fp_state_t state;
  } kw[] = {
    /* Any prefix that our prefix belongs to */
    { "any",   BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY,   VALUE },
    /* Either match this prefix or any more specific prefixes */
    { "more",  BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE,  VALUE },
    /* Either match this prefix or any less specific prefixes */
    { "less",  BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS,  VALUE },
    /* Only match exactly this prefix */
    { "exact", BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT, VALUE },
    { NULL, 0, 0 }
  };

  size_t len = *lenp;
  for (int i = 0; kw[i].word; ++i) {
    if ((strncmp(ext, kw[i].word, len) == 0 && kw[i].word[len] == '\0')) {
      bgpstream_log(BGPSTREAM_LOG_FINE, "Got a '%s' prefix", kw[i].word);
      curr->termtype = kw[i].termtype;
      return *state = kw[i].state;
    }
  }

  /* At this point, assume we're looking at a value instead */
  return bgpstream_parse_value(ext, lenp, state, curr);
}

static int bgpstream_parse_endvalue(const char *conj, size_t len,
    fp_state_t *state, bgpstream_filter_item_t **curr)
{

  if (*curr) {
    free((*curr)->value);
    free(*curr);
    *curr = NULL;
  }

  /* Check for a valid conjunction */
  if (strncmp(conj, "and", len) == 0 && len == 3) {
    *state = TERM;
  } else {
    /* TODO allow 'or', anything else? */
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Bad conjunction in bgpstream filter string: '%s'", conj);
    return *state = FAIL;
  }

  *curr = (bgpstream_filter_item_t *)calloc(1, sizeof(bgpstream_filter_item_t));
  return *state;
}

int bgpstream_parse_filter_string(bgpstream_t *bs, const char *fstring)
{

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

    switch (state) {
    case TERM:
      if (bgpstream_parse_filter_term(p, len, &state, filteritem) == FAIL) {
        goto endparsing;
      }
      break;

    case PREFIXEXT:
      if (bgpstream_parse_prefixext(p, &len, &state, filteritem) == FAIL) {
        goto endparsing;
      }
      if (state == ENDVALUE) {
        instantiate_filter(bs, filteritem);
      }
      break;

    case VALUE:
      if (bgpstream_parse_value(p, &len, &state, filteritem) == FAIL) {
        goto endparsing;
      }
      instantiate_filter(bs, filteritem);
      break;

    case ENDVALUE:
      if (bgpstream_parse_endvalue(p, len, &state, &filteritem) == FAIL) {
        goto endparsing;
      }
      break;

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
