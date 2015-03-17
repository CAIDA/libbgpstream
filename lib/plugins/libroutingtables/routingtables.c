/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2015 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "utils.h"

#include "routingtables.h"
#include "routingtables_int.h"


/* ========== PRIVATE FUNCTIONS ========== */

static char *
graphite_safe(char *p)
{
  if(p == NULL)
    {
      return p;
    }

  char *r = p;
  while(*p != '\0')
    {
      if(*p == '.')
	{
	  *p = '_';
	}
      if(*p == '*')
	{
	  *p = '-';
	}
      p++;
    }
  return r;
}

static uint32_t
get_wall_time_now()
{
  struct timeval tv;
  gettimeofday_wrap(&tv);
  return tv.tv_sec;
}

/** @note for the future
 *  In order to save memory we could use the reserved AS numbers
 *  to embed other informations associated with an AS number, i.e.:
 *  - origin as = 0 -> prefix not seen in the RIB (e.g. withdrawal)
 *  - AS in [64496-64511] -> AS is actually an AS set
 *  - AS in [64512-65534] -> AS is actually an AS confederation  
 */
/* static void asn_mgmt_fun() */
/* { */
  /* http://www.iana.org/assignments/as-numbers/as-numbers.xhtml */
  /* 0	           Reserved */
  /* 64198-64495   Reserved by the IANA */ 
  /* 23456	   AS_TRANS RFC6793 */
  /* 64496-64511   Reserved for use in documentation and sample code RFC5398 */
  /* 64512-65534   Reserved for Private Use RFC6996 */
  /* 65535	   Reserved RFC7300 */
/* } */


/** Returns the origin AS when the origin AS number
 *  numeric, it returns 65535 when the origin is
 *  either a set or a confederation 
 *
 *  @param aspath a pointer to a RIB or ANNOUNCEMENT aspath
 *  @return the origin AS number
 */
static uint32_t
get_origin_asn(bgpstream_as_path_t *aspath)
{
  uint32_t asn = 0;
  bgpstream_as_hop_t as_hop;
  /* populate correctly the asn */
  bgpstream_as_hop_init(&as_hop);
  bgpstream_as_path_get_origin_as(aspath, &as_hop);
  if(as_hop.type == BGPSTREAM_AS_TYPE_NUMERIC)
    {
      asn = as_hop.as_number;
    }
  else
    {
      /* use a reserved AS number to indicate 
       * a set/confederation */
      asn = 65535; 
    }
  bgpstream_as_hop_clear(&as_hop);
  return asn;
}

static perpfx_perpeer_info_t *
perpfx_perpeer_info_create(uint32_t bgp_time_last_ts,
                           uint16_t bgp_time_uc_delta_ts,
                           uint32_t uc_origin_asn)
{
  perpfx_perpeer_info_t *pfxpeeri = (perpfx_perpeer_info_t *) malloc_zero(sizeof(perpfx_perpeer_info_t));
  if(pfxpeeri != NULL)
    {
      pfxpeeri->bgp_time_last_ts = bgp_time_last_ts;
      pfxpeeri->bgp_time_uc_delta_ts = bgp_time_uc_delta_ts;
      pfxpeeri->uc_origin_asn = uc_origin_asn;
    }
  return pfxpeeri;
}

static perpeer_info_t *
perpeer_info_create(uint32_t peer_asnumber,
                    bgpstream_ip_addr_t *peer_ip,
                    bgpstream_elem_peerstate_t bgp_fsm_state,
                    uint32_t bgp_time_ref_rib_start, uint32_t bgp_time_ref_rib_end,
                    uint32_t bgp_time_uc_rib_start,  uint32_t bgp_time_uc_rib_end)
{
  char ip_str[INET_ADDRSTRLEN];
  perpeer_info_t *peeri = (perpeer_info_t *) malloc_zero(sizeof(perpeer_info_t));
  if(peeri != NULL)
    {
      bgpstream_addr_ntop(ip_str, INET_ADDRSTRLEN, peer_ip);
      graphite_safe(ip_str);  
      if(snprintf(peeri->peer_str, BGPSTREAM_UTILS_STR_NAME_LEN,
                  "%"PRIu32".%s", peer_asnumber, ip_str) >= BGPSTREAM_UTILS_STR_NAME_LEN)
        {
          fprintf(stderr, "Warning: could not print peer signature: truncated output\n");
        }
      peeri->bgp_fsm_state = bgp_fsm_state;
      peeri->bgp_time_ref_rib_start = bgp_time_ref_rib_start;
      peeri->bgp_time_ref_rib_end = bgp_time_ref_rib_end;
      peeri->bgp_time_uc_rib_start = bgp_time_uc_rib_start;
      peeri->bgp_time_uc_rib_end = bgp_time_uc_rib_end;
    }
  return peeri;
}

/** @note:
 *  all the xxx_info_create functions do not allocate dynamic
 *  memory other than the structure itself, therefore free() 
 *  is enough to dealloc safely their memory.
 */


static collector_t *
get_collector_data(collector_data_t *collectors, char *project, char *collector)
{
  khiter_t k;
  int khret;
  collector_t c_data;
  
  // create new collector-related structures if it is the first time
  // we see it
  if((k = kh_get(collector_data, collectors, collector))
     == kh_end(collectors))
    {

      // collector data initialization
      
      char project_name[BGPSTREAM_UTILS_STR_NAME_LEN];
      strncpy(project_name, project, BGPSTREAM_UTILS_STR_NAME_LEN);
      graphite_safe(project_name);  

      char collector_name[BGPSTREAM_UTILS_STR_NAME_LEN];
      strncpy(collector_name, collector, BGPSTREAM_UTILS_STR_NAME_LEN);
      graphite_safe(collector_name);  

      if(snprintf(c_data.collector_str , BGPSTREAM_UTILS_STR_NAME_LEN,
                  "%s.%s", project_name, collector_name) >= BGPSTREAM_UTILS_STR_NAME_LEN)
        {
          fprintf(stderr, "Warning: could not print collector signature: truncated output\n");
        }
      
      if((c_data.collector_peerids = kh_init(peer_id_set)) == NULL)
        {
          return NULL;
        }

      c_data.bgp_time_last = 0;
      c_data.wall_time_last = 0;
      c_data.bgp_time_ref_rib_dump_time = 0;
      c_data.bgp_time_ref_rib_start_time = 0;
      c_data.bgp_time_uc_rib_dump_time = 0;
      c_data.bgp_time_uc_rib_start_time = 0;
      c_data.state = ROUTINGTABLES_COLLECTOR_STATE_UNKNOWN;
      
      k = kh_put(collector_data, collectors, strdup(collector), &khret);
      kh_val(collectors,k) = c_data;
    }
  
  return &kh_val(collectors,k);
}



/** Stop the under construction process
 *  @note: this function does not deactivate the peer-pfx fields, 
 *  the peer may be active */
static void
stop_uc_process(routingtables_t *rt, collector_t *c)
{  
  perpeer_info_t *p;
  perpfx_perpeer_info_t *pp;
  
  for(bgpwatcher_view_iter_first_pfx_peer(rt->iter, 0,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_pfx_peer(rt->iter);
      bgpwatcher_view_iter_next_pfx_peer(rt->iter))
    {
      
      /* check if the current field refers to a peer to reset */      
      if(kh_get(peer_id_set, c->collector_peerids, bgpwatcher_view_iter_peer_get_peer(rt->iter)) !=
         kh_end(c->collector_peerids))
        {
          /* the peer belongs to the collector's peers */
          pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
          pp->bgp_time_uc_delta_ts = 0;
          pp->uc_origin_asn = 0;              
          if(bgpwatcher_view_iter_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_INACTIVE)
            {
              bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, 0);
              pp->bgp_time_last_ts = 0;
            }
        }
    }

  /* reset all the uc information for the peers */
  for(bgpwatcher_view_iter_first_peer(rt->iter, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(rt->iter);
      bgpwatcher_view_iter_next_peer(rt->iter))
    {
      /* check if the current field refers to a peer to reset */
      if(kh_get(peer_id_set, c->collector_peerids, bgpwatcher_view_iter_peer_get_peer(rt->iter)) !=
         kh_end(c->collector_peerids))
        {
          p = bgpwatcher_view_iter_peer_get_user(rt->iter);
          p->bgp_time_uc_rib_start = 0;
          p->bgp_time_uc_rib_end = 0;
        }
    }

  /* reset all the uc information for the  collector */
  c->bgp_time_uc_rib_dump_time = 0;
  c->bgp_time_uc_rib_start_time = 0;
}

/** Reset all the pfxpeer data associated with the
 *  provided peer id 
 *  @note: this is the function to call when putting a peer down*/
static void
reset_peerpfxdata(routingtables_t *rt,
                  bgpstream_peer_id_t peer_id, uint8_t reset_uc)
{
  perpfx_perpeer_info_t *pp;
  if(bgpwatcher_view_iter_seek_peer(rt->iter,
                                    peer_id,
                                    BGPWATCHER_VIEW_FIELD_ALL_VALID) == 1)
    {
      for(bgpwatcher_view_iter_first_pfx_peer(rt->iter, 0,
                                              BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                              BGPWATCHER_VIEW_FIELD_ALL_VALID);
          bgpwatcher_view_iter_has_more_pfx_peer(rt->iter);
          bgpwatcher_view_iter_next_pfx_peer(rt->iter))
        {
          if(bgpwatcher_view_iter_peer_get_peer(rt->iter) == peer_id)
            {
              bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, 0);
              pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
              pp->bgp_time_last_ts = 0;
              if(reset_uc)
                {
                  pp->bgp_time_uc_delta_ts = 0;
                  pp->uc_origin_asn = 0;
                }
              bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
            }
        }
    }  
}

static int
end_of_valid_rib(routingtables_t *rt, collector_t *c)
{
  perpeer_info_t *p;
  perpfx_perpeer_info_t *pp;

  for(bgpwatcher_view_iter_first_pfx_peer(rt->iter, 0,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_pfx_peer(rt->iter);
      bgpwatcher_view_iter_next_pfx_peer(rt->iter))
    {
      p = bgpwatcher_view_iter_peer_get_user(rt->iter);
      
      /* check if the current field refers to a peer involved
       * in the rib process  */
     if(kh_get(peer_id_set, c->collector_peerids, bgpwatcher_view_iter_peer_get_peer(rt->iter)) !=
        kh_end(c->collector_peerids)  &&
        p->bgp_time_uc_rib_start != 0)
        {
          pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
          if(pp->bgp_time_uc_delta_ts + p->bgp_time_uc_rib_start >
             pp->bgp_time_last_ts)
            {
              pp->bgp_time_last_ts = pp->bgp_time_uc_delta_ts + p->bgp_time_uc_rib_start;
              bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, pp->uc_origin_asn);
              bgpwatcher_view_iter_pfx_activate_peer(rt->iter);
            }
          else
            {
              /* if an update is more recent than the uc information, then
               * we decide to keep this data and activate the field if it
               * is an announcement */
              if(bgpwatcher_view_iter_pfx_peer_get_orig_asn(rt->iter) != 0)
                {
                  bgpwatcher_view_iter_pfx_activate_peer(rt->iter);
                }
            }
          /* reset uc fields anyway */
          pp->bgp_time_uc_delta_ts = 0;
          pp->uc_origin_asn = 0;
        }

    }
  
  /* reset all the uc information for the peers */
  for(bgpwatcher_view_iter_first_peer(rt->iter, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(rt->iter);
      bgpwatcher_view_iter_next_peer(rt->iter))
    {
      /* check if the current field refers to a peer to reset */
      if(kh_get(peer_id_set, c->collector_peerids, bgpwatcher_view_iter_peer_get_peer(rt->iter)) !=
         kh_end(c->collector_peerids))        
        {
          p = bgpwatcher_view_iter_peer_get_user(rt->iter);
          p->bgp_time_uc_rib_start = 0;
          p->bgp_time_uc_rib_end = 0;
        }
    }
  
  /* reset all the uc information for the  collector */
  c->bgp_time_ref_rib_dump_time = c->bgp_time_uc_rib_dump_time;
  c->bgp_time_ref_rib_start_time = c->bgp_time_uc_rib_start_time;
  c->bgp_time_uc_rib_dump_time = 0;
  c->bgp_time_uc_rib_start_time = 0;
  
  return 0;
}


/** Apply an announcement update or a withdrawal update
 *  @param peer_id peer affected by the update
 *  @param asn     origin 
 *  @return 0 if it finishes correctly, < 0 if something
 *          went wrong
 *
 *  Prerequisites: 
 *  the peer exists and it is either active or inactive
 *  the current iterator points at the right peer
 *  the update time >= collector->bgp_time_ref_rib_start_time
 */
static int
apply_prefix_update(routingtables_t *rt, bgpstream_peer_id_t peer_id,
                    bgpstream_elem_t *elem, uint32_t ts)
{
  perpeer_info_t *p = bgpwatcher_view_iter_peer_get_user(rt->iter);
  perpfx_perpeer_info_t *pp;
  uint32_t asn = 0;           /* 0 for withdrawals, set for announcements */

  /* populate correctly the asn if it is an announcement */
  if(elem->type == BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT)
    {
      asn = get_origin_asn(&elem->aspath);
    }

  /* peer is active */
  if(bgpwatcher_view_iter_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      if(bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                            (bgpstream_pfx_t *) &elem->prefix,
                                            peer_id,
                                            BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                            BGPWATCHER_VIEW_FIELD_ALL_VALID) == 1)
        {
          /* the peer is active, the prefix-peer exists */
          pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
          assert(pp);
          
          if(ts < pp->bgp_time_last_ts)
            {
              /* the update is old and it does not change the state */
              return 0;
            }

          pp->bgp_time_last_ts = ts;
          bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, asn);

          /* the announcement moved the state from inactive to active */
          if(bgpwatcher_view_iter_pfx_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_INACTIVE &&
             asn != 0)
            {
              bgpwatcher_view_iter_pfx_activate_peer(rt->iter);
              return 0;
            }

          /* the withdrawal moved the state from active to inactive */
          if(bgpwatcher_view_iter_pfx_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_ACTIVE &&
             asn == 0)
            {
              bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
              return 0;
            }
          
          /* the update did not move the state of the pfx peer field */
          return 0;
        }
      else
        {
          /* the peer is active, the prefix-peer does not exist */
          bgpwatcher_view_add_pfx_peer(rt->view,
                                       (bgpstream_pfx_t *) &elem->prefix,
                                       peer_id,
                                       asn);
          bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                             (bgpstream_pfx_t *) &elem->prefix,
                                             peer_id,
                                             BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                             BGPWATCHER_VIEW_FIELD_ALL_VALID);
          pp = perpfx_perpeer_info_create(ts, 0, 0);
          assert(pp);
          bgpwatcher_view_iter_pfx_peer_set_user(rt->iter, pp);
          /* bgpwatcher_view_add_pfx_peer automatically set the active
           *  state, if the case was withdrawal we need to deactivate the field */
          if(asn == 0)
            {
              bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
            }
          return 0;
        }
    }
  else
    {
      if(bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                            (bgpstream_pfx_t *) &elem->prefix,
                                            peer_id,
                                            BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                            BGPWATCHER_VIEW_FIELD_ALL_VALID) == 1)
        {
          /* the peer is inactive, the prefix-peer exists */
          pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
          if(ts < pp->bgp_time_last_ts)
            {
              /* the update is old and it does not change the state */
              return 0;
            }
          
          if(bgpwatcher_view_iter_pfx_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_INACTIVE)
            {
              /* the peer is inactive, the prefix-peer exists and it is inactive */
              
              /* case 1: the peer is inactive because its state is unknown and there is
               * an under construction process going on */
              if(p->bgp_fsm_state == BGPSTREAM_ELEM_PEERSTATE_UNKNOWN &&
                 p->bgp_time_uc_rib_start != 0)
                {
                  /* the peer remains inactive, the information contained in the
                   * pfx-peer will be used when the uc rib becomes active */
                  pp->bgp_time_last_ts = ts;
                  bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, asn);
                  return 0;
                }

              /* case 2: the peer is inactive because its fsm state went down,
               * if we receive a new update we assume the state is established 
               * and the peer is up again  */
              if(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_UNKNOWN)
                {
                  /* the peer goes active */
                  bgpwatcher_view_iter_activate_peer(rt->iter);
                  p->bgp_fsm_state = BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED;
                  p->bgp_time_ref_rib_start = ts;
                  p->bgp_time_ref_rib_end = ts;
                  pp->bgp_time_last_ts = ts;
                  bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, asn);
                  if(asn != 0)
                    {
                      bgpwatcher_view_iter_pfx_activate_peer(rt->iter);
                    }
                  return 0;
                }

              /* if a peer is inactive it's state has to be not ESTABLISHED
              *  it is possible that the state is UNKNOWN, but no uc process
              *  is on (e.g. after a corrupted read), in this case nothing
              *  has to change */
              return 0;
            }
          else
            {
              /* the peer is inactive, the prefix-peer exists and it is active:
               * programming error, a pfx peer can't be active if the peer is inactive */
              assert(0); 
            }
        }
      else
        {
          /* the peer is inactive, the prefix-peer does not exist, we have to
           * create it*/
          bgpwatcher_view_add_pfx_peer(rt->view,
                                       (bgpstream_pfx_t *) &elem->prefix,
                                       peer_id,
                                       asn);
          bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                             (bgpstream_pfx_t *) &elem->prefix,
                                             peer_id,
                                             BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                             BGPWATCHER_VIEW_FIELD_ALL_VALID);

            /* case 1: the peer is inactive because its state is unknown and there is
             * an under construction process going on */
            if(p->bgp_fsm_state == BGPSTREAM_ELEM_PEERSTATE_UNKNOWN &&
               p->bgp_time_uc_rib_start != 0)
              {
                /* the peer should remain inactive, the information contained in the
                 * pfx-peer will be used when the uc rib becomes active */
                pp = perpfx_perpeer_info_create(ts, 0, 0);
                assert(pp);
                bgpwatcher_view_iter_pfx_peer_set_user(rt->iter, pp);
                bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
                bgpwatcher_view_iter_deactivate_peer(rt->iter);
                return 0;
              }

          /* case 2: the peer is inactive because its fsm state went down,
           * if we receive a new update we assume the state is established 
           * and the peer is up again  */
          if(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_UNKNOWN)
            {
              /* the peer goes active (add_pfx_peer already did that) */
              pp = perpfx_perpeer_info_create(ts, 0, 0);
              assert(pp);
              bgpwatcher_view_iter_pfx_peer_set_user(rt->iter, pp);              
              p->bgp_fsm_state = BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED;
              p->bgp_time_ref_rib_start = ts;
              p->bgp_time_ref_rib_end = ts;
              if(asn != 0)
                {
                  bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
                }
              return 0;
            }

          /* if a peer is inactive it's state has to be not ESTABLISHED
           *  it is possible that the state is UNKNOWN, but no uc process
           *  is on (e.g. after a corrupted read), in this case nothing
           *  has to change */
          return 0;
        }
    }
  return 0;
}


static int
apply_state_update(routingtables_t *rt, bgpstream_peer_id_t peer_id,
                   bgpstream_elem_peerstate_t new_state, uint32_t ts)
{

  if(new_state == BGPSTREAM_ELEM_PEERSTATE_UNKNOWN || new_state >= BGPSTREAM_ELEM_PEERSTATE_NULL)
    {
      // @todo BGPSTREAM_ELEM_PEERSTATE_NULL occurs, check in bgpstream what is going on!
      return 0;
    }
  /* debug printf("Peer %d applying state: %d\n", peer_id, new_state); */
  perpeer_info_t *p;
  bgpstream_peer_sig_t *sg;
  if(bgpwatcher_view_iter_seek_peer(rt->iter,
                                    peer_id,
                                    BGPWATCHER_VIEW_FIELD_ALL_VALID) != 1)
    {      
      /* peer does not exist, create */
      sg = bgpstream_peer_sig_map_get_sig(rt->peersigns, peer_id);
      p = perpeer_info_create(sg->peer_asnumber, (bgpstream_ip_addr_t *) &sg->peer_ip_addr,
                              new_state, ts, ts, 0, 0);
      bgpwatcher_view_add_peer(rt->view, sg->collector_str, (bgpstream_ip_addr_t *) &sg->peer_ip_addr, sg->peer_asnumber);
      bgpwatcher_view_iter_seek_peer(rt->iter, peer_id, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_peer_set_user(rt->iter, p);
      if(new_state != BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED)
        {
          bgpwatcher_view_iter_deactivate_peer(rt->iter);
        }
      return 0;
    }
  else
    {
      /* the peer is active/inactive */      
      p = bgpwatcher_view_iter_peer_get_user(rt->iter);
      assert(p);
      
      if(bgpwatcher_view_iter_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_ACTIVE)
        {
          assert(p->bgp_fsm_state == BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED);
        }
      else
        {
          assert(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED);
        }

      if(p->bgp_fsm_state == BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED &&
         new_state != BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED)
        {
          /* the peer is active and we receive a peer down message */
          p->bgp_fsm_state = new_state;
          p->bgp_time_ref_rib_start = ts;
          p->bgp_time_ref_rib_end = ts;
          uint8_t reset_uc = 0;
          if(ts >= p->bgp_time_uc_rib_start)
            {
              reset_uc = 1;
              p->bgp_time_uc_rib_start = 0;
              p->bgp_time_uc_rib_end = 0;
            }
          /* reset all peer pfx data associated with the peer */
          reset_peerpfxdata(rt, peer_id, reset_uc);
          bgpwatcher_view_iter_deactivate_peer(rt->iter);
          return 0;
        }      

      if(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED &&
         new_state == BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED)
        {
          /* the peer is inactive and we receive a peer up message */
          p->bgp_fsm_state = new_state;
          p->bgp_time_ref_rib_start = ts;
          p->bgp_time_ref_rib_end = ts;
          bgpwatcher_view_iter_activate_peer(rt->iter);
          return 0;
        }

      /* update the state anyway */
      if(p->bgp_fsm_state != new_state)
        {
          p->bgp_fsm_state = new_state;
          p->bgp_time_ref_rib_start = ts;
          p->bgp_time_ref_rib_end = ts;
          return 0;
        }

      assert(p->bgp_fsm_state == BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED &&
             new_state == BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED);

    }
  return 0;
}

static int
apply_rib_message(routingtables_t *rt, bgpstream_peer_id_t peer_id,
                  bgpstream_elem_t *elem, uint32_t ts)
{
  perpeer_info_t *p;
  bgpstream_peer_sig_t *sg;
  bgpwatcher_view_field_state_t peer_init_state;

  if(bgpwatcher_view_iter_seek_peer(rt->iter,
                                    peer_id,
                                    BGPWATCHER_VIEW_FIELD_ALL_VALID) != 1)
    {
      /* the peer does not exist, we create it */  
      sg = bgpstream_peer_sig_map_get_sig(rt->peersigns, peer_id);
      p = perpeer_info_create(sg->peer_asnumber, (bgpstream_ip_addr_t *) &sg->peer_ip_addr,
                              BGPSTREAM_ELEM_PEERSTATE_UNKNOWN, 0, 0, ts, ts);
      bgpwatcher_view_add_peer(rt->view, sg->collector_str,
                               (bgpstream_ip_addr_t *) &sg->peer_ip_addr,
                               sg->peer_asnumber);
      bgpwatcher_view_iter_seek_peer(rt->iter, peer_id, BGPWATCHER_VIEW_FIELD_ALL_VALID);      
      bgpwatcher_view_iter_peer_set_user(rt->iter, p);
      /* a rib message cannot activate a peer */
      bgpwatcher_view_iter_deactivate_peer(rt->iter);
    }
  else
    {
      p = bgpwatcher_view_iter_peer_get_user(rt->iter);
      if(p->bgp_time_uc_rib_start == 0)
        {
          p->bgp_time_uc_rib_start = ts;
        }
      p->bgp_time_uc_rib_end = ts;
    }

  peer_init_state = bgpwatcher_view_iter_peer_get_state(rt->iter);

  
  perpfx_perpeer_info_t *pp;
  uint32_t asn = get_origin_asn(&elem->aspath);           
  
  if(bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                        (bgpstream_pfx_t *) &elem->prefix,
                                        peer_id,
                                        BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                        BGPWATCHER_VIEW_FIELD_ALL_VALID) == 1)
    {
      pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);
      pp->bgp_time_uc_delta_ts = ts - p->bgp_time_uc_rib_start;
      pp->uc_origin_asn = asn;
    }
  else
    {

      bgpwatcher_view_add_pfx_peer(rt->view,
                                   (bgpstream_pfx_t *) &elem->prefix,
                                   peer_id,
                                   0);            
      bgpwatcher_view_iter_seek_pfx_peer(rt->iter,
                                         (bgpstream_pfx_t *) &elem->prefix,
                                         peer_id,
                                         BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                         BGPWATCHER_VIEW_FIELD_ALL_VALID);
      pp = perpfx_perpeer_info_create(0, ts - p->bgp_time_uc_rib_start, asn);
      bgpwatcher_view_iter_pfx_peer_set_user(rt->iter,pp);
      bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
      if(peer_init_state == BGPWATCHER_VIEW_FIELD_INACTIVE)
        {
          bgpwatcher_view_iter_deactivate_peer(rt->iter);
        }
    }

  return 0;
}


static void
update_collector_state(routingtables_t *rt,
                       collector_t *c,
                       bgpstream_record_t *record)
{
  /** we update the bgp_time_last and every ROUTINGTABLES_COLLECTOR_WALL_UPDATE_FR 
   *  seconds we also update the last wall time */
  if(record->attributes.record_time > c->bgp_time_last)
    {
      if(record->attributes.record_time >
         (c->bgp_time_last + ROUTINGTABLES_COLLECTOR_WALL_UPDATE_FR))
        {
          c->wall_time_last = get_wall_time_now();
        }
      c->bgp_time_last = record->attributes.record_time;      
    }

  /** we update the status of the collector based on the state of its peers 
   * a collector is in an unknown state if all of its peers
   * are in an unknown state, it is down if all of its peers 
   * states are either down or unknown, it is up if at least
   * one peer is up */
  
  perpeer_info_t *p;  
  uint8_t unknown = 1;
  for(bgpwatcher_view_iter_first_peer(rt->iter, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(rt->iter);
      bgpwatcher_view_iter_next_peer(rt->iter))
    {
        if(kh_get(peer_id_set, c->collector_peerids, bgpwatcher_view_iter_peer_get_peer(rt->iter)) !=
           kh_end(c->collector_peerids))
          {
          switch(bgpwatcher_view_iter_peer_get_state(rt->iter))
            {
            case BGPWATCHER_VIEW_FIELD_ACTIVE:
              c->state = ROUTINGTABLES_COLLECTOR_STATE_UP;
              return; 
            case BGPWATCHER_VIEW_FIELD_INACTIVE:
              p = bgpwatcher_view_iter_peer_get_user(rt->iter);
              if(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_UNKNOWN)
                {
                  unknown = 0;
                }
              break;
            default:
              /* a valid peer cannot be in state invalid */
              assert(0);
            }
        }
    }

  if(unknown == 1)
    {
      c->state = ROUTINGTABLES_COLLECTOR_STATE_DOWN;
    }
  else
    {
      c->state = ROUTINGTABLES_COLLECTOR_STATE_UNKNOWN;
    }
  
  return;
}


/* debug static int peerscount = 0; */

static int
collector_process_valid_bgpinfo(routingtables_t *rt,
                                collector_t *c,
                                bgpstream_record_t *record)
{
  int ret;
  bgpstream_elem_t *elem;
  bgpstream_peer_id_t peer_id;  
  int khret;
  khiter_t k;
  
  if(record->attributes.dump_type == BGPSTREAM_UPDATE)
    {      
      while((elem = bgpstream_record_get_next_elem(record)) != NULL)
        {
          /* get the peer id (create one if it doesn't exist) */
          if((peer_id = bgpstream_peer_sig_map_get_id(rt->peersigns,
                                                      record->attributes.dump_collector,
                                                      &elem->peer_address,
                                                      elem->peer_asnumber)) == 0)
            {
              return -1;                 
            }

          /* insert the peer id in the collector peer ids set */
          if((k = kh_get(peer_id_set, c->collector_peerids, peer_id)) == kh_end(c->collector_peerids))
            {
              k = kh_put(peer_id_set, c->collector_peerids, peer_id, &khret);
            }
          
          switch(elem->type)
            {
            case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
            case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
              /* the peer must exist in order to apply an update
               * if not, the message will be discarded */
              if(bgpwatcher_view_iter_seek_peer(rt->iter,
                                                peer_id,
                                                BGPWATCHER_VIEW_FIELD_ALL_VALID) == 1)
                {
                  ret = apply_prefix_update(rt, peer_id, elem, record->attributes.record_time);
                }
              break;
            case BGPSTREAM_ELEM_TYPE_PEERSTATE:            
              ret = apply_state_update(rt, peer_id, elem->new_state, record->attributes.record_time);
                /* debug if(peerscount != bgpwatcher_view_peer_size(rt->view)) */
                /*   { */
                /*     peerscount = bgpwatcher_view_peer_size(rt->view); */
                /*     printf("\t\t\tActive peers: %"PRIu32"\n", peerscount); */
                /*   } */
              break;
            default:
              /* programming error or bgpstream bug */
              assert(0);      
            }
        }
    }

  else {
    
    if(record->attributes.dump_type == BGPSTREAM_RIB)
      {
        /* start a new RIB construction process if there is a 
         * new START message */
        if(record->dump_pos == BGPSTREAM_DUMP_START)
          {
            /* if there is already another under construction 
             * process going on, then we have to reset the process */
            if(c->bgp_time_uc_rib_dump_time)
              {
                stop_uc_process(rt, c);
              }
            c->bgp_time_uc_rib_dump_time = record->attributes.dump_time;
            c->bgp_time_uc_rib_start_time = record->attributes.record_time;
          }

        /* we process RIB information (ALL of them: start,middle,end)
         * only if there is an under construction process that refers
         * to the same RIB dump */
        if(record->attributes.dump_time == c->bgp_time_uc_rib_dump_time)
          {
         
            while((elem = bgpstream_record_get_next_elem(record)) != NULL)
              {
                /* get the peer id (create one if it doesn't exist) */
                if((peer_id = bgpstream_peer_sig_map_get_id(rt->peersigns,
                                                            record->attributes.dump_collector,
                                                            &elem->peer_address,
                                                            elem->peer_asnumber)) == 0)
                  {
                    return -1;                 
                  }
                
                /* insert the peer id in the collector peer ids set */
                if((k = kh_get(peer_id_set, c->collector_peerids, peer_id)) == kh_end(c->collector_peerids))
                  {
                    k = kh_put(peer_id_set, c->collector_peerids, peer_id, &khret);
                  }
                
                /* apply the rib message */
                apply_rib_message(rt, peer_id, elem, record->attributes.record_time);
              }

            if(record->dump_pos == BGPSTREAM_DUMP_END)
              {
                /* promote the current uc information to active
                 * information and reset the uc info */
                end_of_valid_rib(rt, c);
              }          
          }          
      }
    else
      {
        /* programming error or bgpstream bug */
        assert(0);      
      }
  }
  
    
  return 0;
}


static int
collector_process_corrupted_message(routingtables_t *rt,
                                    collector_t *c,
                                    bgpstream_record_t *record)
{
  khiter_t k;
  bgpstream_peer_id_t peer_id;  
  perpeer_info_t *p;
  /* list of peers whose current active rib is affected by the
   * corrupted message */
  bgpstream_id_set_t *cor_affected = bgpstream_id_set_create();
  /* list of peers whose current under construction rib is affected by the
   * corrupted message */
  bgpstream_id_set_t *cor_uc_affected = bgpstream_id_set_create();
  
  /* get all the peers that belong to the current collector */    
  for(k = kh_begin(c->collector_peerids); k != kh_end(c->collector_peerids); ++k)
    {
      if(kh_exist(c->collector_peerids, k))
	{
          peer_id = kh_key(c->collector_peerids, k);
          bgpwatcher_view_iter_seek_peer(rt->iter, peer_id, BGPWATCHER_VIEW_FIELD_ALL_VALID);
          p = bgpwatcher_view_iter_peer_get_user(rt->iter);
          assert(p);
          /* save all the peers affected by the corrupted record */    
          if(p->bgp_time_ref_rib_start != 0 && record->attributes.record_time >= p->bgp_time_ref_rib_start)
            {
              bgpstream_id_set_insert(cor_affected, peer_id);
            }
          if(p->bgp_time_uc_rib_start != 0 && record->attributes.record_time >= p->bgp_time_uc_rib_start)
            {
              bgpstream_id_set_insert(cor_uc_affected, peer_id);
            }
        }
    }

  perpfx_perpeer_info_t *pp;
  
  for(bgpwatcher_view_iter_first_pfx_peer(rt->iter, 0,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID,
                                          BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_pfx_peer(rt->iter);
      bgpwatcher_view_iter_next_pfx_peer(rt->iter))
    {      
      pp = bgpwatcher_view_iter_pfx_peer_get_user(rt->iter);

      if(bgpstream_id_set_exists(cor_affected, bgpwatcher_view_iter_peer_get_peer(rt->iter)))
        {
          if(pp->bgp_time_last_ts !=0 && pp->bgp_time_last_ts <= record->attributes.record_time)
            {
              pp->bgp_time_last_ts = 0;
              bgpwatcher_view_iter_pfx_peer_set_orig_asn(rt->iter, 0);
              bgpwatcher_view_iter_pfx_deactivate_peer(rt->iter);
            }
        }
        
      if(bgpstream_id_set_exists(cor_uc_affected, bgpwatcher_view_iter_peer_get_peer(rt->iter)))
        {
          pp->bgp_time_uc_delta_ts = 0;
          pp->uc_origin_asn = 0;          
        }      
    }

  for(bgpwatcher_view_iter_first_peer(rt->iter, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(rt->iter);
      bgpwatcher_view_iter_next_peer(rt->iter))
    {
      if(bgpstream_id_set_exists(cor_affected, bgpwatcher_view_iter_peer_get_peer(rt->iter)))
        {
          p = bgpwatcher_view_iter_peer_get_user(rt->iter);
          p->bgp_fsm_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
          p->bgp_time_ref_rib_start = 0;
          p->bgp_time_ref_rib_end = 0;
          bgpwatcher_view_iter_deactivate_peer(rt->iter);
        }
      if(bgpstream_id_set_exists(cor_uc_affected, bgpwatcher_view_iter_peer_get_peer(rt->iter)))
        {
          p->bgp_time_uc_rib_start = 0;
          p->bgp_time_uc_rib_end = 0;
        }
    }
  
  return 0;
}



/* ========== PUBLIC FUNCTIONS ========== */

routingtables_t *routingtables_create()
{  
  routingtables_t *rt = (routingtables_t *)malloc_zero(sizeof(routingtables_t));
  if(rt == NULL)
    {
      goto err;
    }

  if((rt->peersigns = bgpstream_peer_sig_map_create() ) == NULL)
    {
      goto err;
    }

  if((rt->view = bgpwatcher_view_create_shared(rt->peersigns,
                                               NULL /* view user destructor */,
                                               NULL /* pfx destructor */,
                                               free /* peer user destructor */,
                                               free /* pfxpeer user destructor */)) == NULL)
    {
      goto err;
    }

  if((rt->iter = bgpwatcher_view_iter_create(rt->view)) == NULL)
   {
      goto err;
    }
  
  if((rt->collectors = kh_init(collector_data)) == NULL)
    {
      goto err;
    }

  // set the metric prefix string to the default value
  routingtables_set_metric_prefix(rt,
                                  ROUTINGTABLES_DEFAULT_METRIC_PFX);

  // set the ff thresholds to their default values
  rt->ipv4_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV4_FULLFEED_THR;
  rt->ipv6_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV6_FULLFEED_THR;

  rt->bgp_time_interval_start = 0;
  rt->bgp_time_interval_end = 0;
  rt->wall_time_interval_start = 0;
    
#ifdef WITH_BGPWATCHER
  rt->watcher_tx_on = 0;
  rt->watcher_client = NULL;
  rt->tables_mask = 0;
#endif  

  return rt;

 err:
  fprintf(stderr, "routingtables_create failed\n");
  routingtables_destroy(rt);
  return NULL;
}

void routingtables_set_metric_prefix(routingtables_t *rt,
                                     char *metric_prefix)
{
  if(metric_prefix == NULL ||
     strlen(metric_prefix)-1 > ROUTINGTABLES_METRIC_PFX_LEN)
    {
      fprintf(stderr,
              "Warning: could not set metric prefix, using default %s \n",
              ROUTINGTABLES_DEFAULT_METRIC_PFX);
      strcpy(rt->metric_prefix, ROUTINGTABLES_DEFAULT_METRIC_PFX);
      return;
    }
  strcpy(rt->metric_prefix, metric_prefix);
}

char *routingtables_get_metric_prefix(routingtables_t *rt)
{
  return &rt->metric_prefix[0];
}

#ifdef WITH_BGPWATCHER
int routingtables_activate_watcher_tx(routingtables_t *rt,
                                      char *client_name,
                                      char *server_uri,
                                      uint8_t tables_mask)
{

  if((rt->watcher_client = bgpwatcher_client_init(0 /* no interests */,
                                                  BGPWATCHER_PRODUCER_INTENT_PREFIX /* peers and pfxs*/
                                                  )) == NULL)
    {
      fprintf(stderr,
              "Error: could not initialize bgpwatcher client\n");
      return -1;
    }

  if(server_uri != NULL &&
     bgpwatcher_client_set_server_uri(rt->watcher_client, server_uri) != 0)
    {
      goto err;
    }

  if(client_name != NULL &&
     bgpwatcher_client_set_identity(rt->watcher_client, client_name) != 0)
    {
      fprintf(stderr,
              "Warning: could not set client identity to %s, using random ID\n",
              client_name);
    }
  
  if(bgpwatcher_client_start(rt->watcher_client) != 0)
    {
      fprintf(stderr,
              "Error: cannot start bgpwatcher client \n");
      goto err;
    }
  
  rt->watcher_tx_on = 1;
  rt->tables_mask = ROUTINGTABLES_ALL_FEEDS; // default: all feeds 
  if(tables_mask != 0)
    {
      rt->tables_mask = tables_mask;
    }

  return 0;

    err:
  if(rt->watcher_client != NULL)
    {
      bgpwatcher_client_perr(rt->watcher_client);
      bgpwatcher_client_free(rt->watcher_client);
    }
    rt->watcher_tx_on = 0;
    rt->watcher_client = NULL;
    return -1; 
}
#endif

void routingtables_set_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version,
                                         uint32_t threshold)
{
  switch(ip_version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      rt->ipv4_fullfeed_th = threshold;
      break;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      rt->ipv6_fullfeed_th = threshold;
      break;
    default:
      /* programming error */
      assert(0);      
    }
}

int routingtables_get_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version)
{
 switch(ip_version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      return rt->ipv4_fullfeed_th;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      return rt->ipv6_fullfeed_th;
    default:
      /* programming error */
      assert(0);      
    }
 return -1;
}

int routingtables_interval_start(routingtables_t *rt,
                                 int start_time)
{
  rt->bgp_time_interval_start = (uint32_t) start_time;
  rt->wall_time_interval_start = get_wall_time_now();
  return 0;
}

int routingtables_interval_end(routingtables_t *rt,
                               int end_time)
{
  rt->bgp_time_interval_end = (uint32_t) end_time;
  uint32_t elapsed_time = get_wall_time_now() - rt->wall_time_interval_start;
  fprintf(stderr, "Interval [%"PRIu32", %"PRIu32"] processed in %"PRIu32"s\n",
          rt->bgp_time_interval_start, rt->bgp_time_interval_end, elapsed_time);

  /** @todo: print statistics and send the view to the watcher if tx is on */

  printf("%d - active peers: %"PRIu32"\n", end_time, bgpwatcher_view_peer_size(rt->view));

  return 0;
}

int routingtables_process_record(routingtables_t *rt,
                                 bgpstream_record_t *record)
{
  int ret = 0;
  collector_t *c;
  
  /* get a pointer to the current collector data, if no data
   * exists yet, a new structure will be created */
  if((c = get_collector_data(rt->collectors,
                             record->attributes.dump_project,
                             record->attributes.dump_collector)) == NULL)
    {
      return -1;
    }

  /* if a record refer to a time prior to the current reference time,
   * then we discard it, unless we are in the process of building a
   * new rib, in that case we check the time against the uc starting
   * time and if it is a prior record we discard it */
  if(record->attributes.record_time < c->bgp_time_ref_rib_start_time)
    {
      if(c->bgp_time_uc_rib_dump_time != 0)
        {
          if(record->attributes.record_time < c->bgp_time_ref_rib_start_time)
            {
              return 0;
            }
        }
    }

  switch(record->status)
    {
    case BGPSTREAM_RECORD_STATUS_VALID_RECORD:
      ret = collector_process_valid_bgpinfo(rt, c, record);
      break;
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE:
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD:
      ret = collector_process_corrupted_message(rt, c, record);
      break;
    case BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE:
    case BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE:
      /** An empty or filtered source does not change the current
       *  state of a collector, however we update the last_ts
       *  observed */
      if(record->attributes.record_time < c->bgp_time_last)
        {
          c->bgp_time_last = record->attributes.record_time;
        }
      break;
    default:
      /* programming error */
      assert(0);
    }
  
  update_collector_state(rt, c, record);

  /* fprintf(stderr, "Processed %s record %ld\n", */
  /*         c->collector_str, */
  /*         record->attributes.record_time); */

  return ret;
}

void routingtables_destroy(routingtables_t *rt)
{
  khiter_t k;
  if(rt != NULL)
    {
      if(rt->collectors != NULL)
        {
          for (k = kh_begin(rt->collectors);
               k != kh_end(rt->collectors); ++k)
            {          
              if (kh_exist(rt->collectors, k))
                {
                  /* deallocating value dynamic memory */
                  kh_destroy(peer_id_set, kh_val(rt->collectors, k).collector_peerids);
                  kh_val(rt->collectors, k).collector_peerids = NULL;
                  /* deallocating string dynamic memory */
                  free(kh_key(rt->collectors, k));
                }
            }   
          kh_destroy(collector_data, rt->collectors );
          rt->collectors = NULL;    
        }
      
      if(rt->iter != NULL)
        {
          bgpwatcher_view_iter_destroy(rt->iter);
          rt->iter = NULL;
        }
            
      if(rt->view != NULL)
        {
          bgpwatcher_view_destroy(rt->view);
          rt->view =NULL;
        }
              
      if(rt->peersigns != NULL)
        {
          bgpstream_peer_sig_map_destroy(rt->peersigns);
          rt->peersigns = NULL;
        }      

#ifdef WITH_BGPWATCHER
      if(rt->watcher_client != NULL)
        {
          bgpwatcher_client_stop(rt->watcher_client);
	  bgpwatcher_client_perr(rt->watcher_client);
	  bgpwatcher_client_free(rt->watcher_client);
          rt->watcher_client = NULL;
        }
#endif

      free(rt);
    }
}

