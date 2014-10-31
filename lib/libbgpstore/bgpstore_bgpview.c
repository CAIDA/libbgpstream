/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "bgpstore_bgpview.h"

static peerview_t* peerview_create()
{
  peerview_t *pv;
  if((pv = malloc_zero(sizeof(peerview_t))) == NULL)
    {
      return NULL;
    }
  if((pv->peer_pfxview = kh_init(bsid_pfxview)) != NULL)
    {
      free(pv);
      pv = NULL;
    }
  return pv;
}


static void peerview_destroy(peerview_t *pv)
{
  if(pv != NULL)
    {
      if(pv->peer_pfxview != NULL)
	{
	  kh_destroy(bsid_pfxview, pv->peer_pfxview);
	  pv->peer_pfxview = NULL;
	}
      free(pv);
    }
}

/* ###################################################### */
/*          coll_status_t related functions               */
/* ###################################################### */

static void coll_status_destroy(coll_status_t *cs)
{
   if(cs != NULL)
    {
      if(cs->active_peer_ids_list != NULL)
	{
	  kh_destroy(id_set, cs->active_peer_ids_list);
	  cs->active_peer_ids_list = NULL;
	}
      if(cs->inactive_peer_ids_list != NULL)
	{
	  kh_destroy(id_set, cs->inactive_peer_ids_list);
	  cs->inactive_peer_ids_list = NULL;
	}      
      free(cs);
    }
}

static coll_status_t *coll_status_create()
{
  coll_status_t *cs;
  if((cs = malloc_zero(sizeof(coll_status_t))) == NULL)
    {
      return NULL;
    }
  if((cs->active_peer_ids_list = kh_init(id_set)) != NULL)
    {
      goto err;
    }
  if((cs->inactive_peer_ids_list = kh_init(id_set)) != NULL)
    {
      goto err;
    }
  return cs;

 err:
  coll_status_destroy(cs);
  return NULL;
}





bgpview_t *bgpview_create()
{
  bgpview_t *bgp_view;

  if((bgp_view = malloc_zero(sizeof(bgpview_t))) == NULL)
    {
      return NULL;
    }

  if((bgp_view->aggregated_pfxview_ipv4 = kh_init(aggr_pfxview_ipv4)) == NULL)
    {
      goto err;
    }

  if((bgp_view->aggregated_pfxview_ipv6 = kh_init(aggr_pfxview_ipv6)) == NULL)
    {
      goto err;
    }

  if((bgp_view->done_clients = bl_string_set_create()) == NULL)
    {
      goto err;
    }

  if((bgp_view->collector_status = kh_init(collectorstr_status)) == NULL)
    {
      goto err;
    }

  if((bgp_view->peer_status = kh_init(id_status)) == NULL)
    {
      goto err;
    }

  return bgp_view;
    
 err:
  fprintf(stderr, "Failed to create bgpstore bgpview\n");
  if(bgp_view != NULL)
    {
      bgpview_destroy(bgp_view);
    }
  return NULL;
}


int bgpview_add_peer(bgpview_t *bgp_view, char *collector, bgpwatcher_peer_t* peer_info)
{
  // TODO
  return 0;
}


int bgpview_add_row(bgpview_t *bgp_view, bgpwatcher_pfx_table_t *table,
		    bgpwatcher_pfx_row_t *row)
{
  // TODO
  return 0;
}


int bgpview_table_end(bgpview_t *bgp_view, char *client_name,
		      bgpwatcher_pfx_table_t *table)
{
  // TODO
  return 0;
}


void bgpview_destroy(bgpview_t *bgp_view)
{
  if(bgp_view != NULL)
    {

      if(bgp_view->aggregated_pfxview_ipv4 != NULL)
	{
	  kh_free_vals(aggr_pfxview_ipv4, bgp_view->aggregated_pfxview_ipv4,
		       peerview_destroy);
	  kh_destroy(aggr_pfxview_ipv4, bgp_view->aggregated_pfxview_ipv4);
	  bgp_view->aggregated_pfxview_ipv4 = NULL;
	}

      if(bgp_view->aggregated_pfxview_ipv6 != NULL)
	{
	  kh_free_vals(aggr_pfxview_ipv6, bgp_view->aggregated_pfxview_ipv6,
		       peerview_destroy);
	  kh_destroy(aggr_pfxview_ipv6, bgp_view->aggregated_pfxview_ipv6);
	  bgp_view->aggregated_pfxview_ipv6 = NULL;
	}

      if(bgp_view->done_clients != NULL)
	{
	  bl_string_set_destroy(bgp_view->done_clients);
	  bgp_view->done_clients = NULL;
	}

      if(bgp_view->collector_status != NULL)
	{
	  kh_free_vals(collectorstr_status, bgp_view->collector_status, coll_status_destroy);
	  kh_destroy(collectorstr_status, bgp_view->collector_status);
	  bgp_view->collector_status = NULL;
	}
      
      if(bgp_view->peer_status != NULL)
	{
	  kh_destroy(id_status, bgp_view->peer_status);
	  bgp_view->peer_status = NULL;
	}

      free(bgp_view);
    }
}

