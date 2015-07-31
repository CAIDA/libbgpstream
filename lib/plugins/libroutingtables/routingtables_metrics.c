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

/* <metric-prefix>.<plugin-name>.<collector-signature>.<metric-name> */
#define RT_COLLECTOR_METRIC_FORMAT "%s.%s.%s.%s"

/* <metric-prefix>.meta.bgpcorsaro.<plugin-name>.<collector-signature>.<metric-name> */
#define RT_COLLECTOR_META_METRIC_FORMAT "%s.meta.bgpcorsaro.%s.%s.%s"

/* <metric-prefix>.<plugin-name>.<collector-signature>.<peer-signature>.<metric-name> */
#define RT_PEER_METRIC_FORMAT "%s.%s.%s.%s.%s"

/* <metric-prefix>.meta.bgpcorsaro.<plugin-name>.<collector-signature>.<peer-signature>.<metric-name> */
#define RT_PEER_META_METRIC_FORMAT "%s.meta.bgpcorsaro.%s.%s.%s.%s"

#define BUFFER_LEN 1024
static char metric_buffer[BUFFER_LEN];

static uint32_t
add_p_metric(timeseries_kp_t *kp, char *metric_prefix, char *plugin_name, char *c_sig, char *p_sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_PEER_METRIC_FORMAT, metric_prefix, plugin_name, c_sig, p_sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

static uint32_t
add_meta_p_metric(timeseries_kp_t *kp, char *metric_prefix, char *plugin_name, char *c_sig, char *p_sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_PEER_META_METRIC_FORMAT, metric_prefix, plugin_name, c_sig, p_sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

void 
peer_generate_metrics(routingtables_t *rt, perpeer_info_t *p)
{
  p->kp_idxs.status_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "status");
  p->kp_idxs.active_v4_pfxs_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "active_v4_pfxs_cnt"); 
  p->kp_idxs.active_v6_pfxs_idx =                                       \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "active_v6_pfxs_cnt");
   
  p->kp_idxs.announcing_origin_as_idx =                                 \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "unique_announcing_origin_ases_cnt");
  p->kp_idxs.announced_v4_pfxs_idx =                                    \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "announced_v4_unique_pfxs_cnt");
  p->kp_idxs.withdrawn_v4_pfxs_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "withdrawn_v4_unique_pfxs_cnt");
  p->kp_idxs.announced_v6_pfxs_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "announced_v6_unique_pfxs_cnt");
  p->kp_idxs.withdrawn_v6_pfxs_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "withdrawn_v6_unique_pfxs_cnt");

  p->kp_idxs.rib_messages_cnt_idx =                                     \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "rib_messages_cnt");
  p->kp_idxs.pfx_announcements_cnt_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "announcements_cnt");
  p->kp_idxs.pfx_withdrawals_cnt_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "withdrawals_cnt");
  p->kp_idxs.state_messages_cnt_idx = \
    add_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "state_messages_cnt");

  p->kp_idxs.inactive_v4_pfxs_idx =                                     \
    add_meta_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "inactive_v4_pfxs_cnt");
  p->kp_idxs.inactive_v6_pfxs_idx = \
    add_meta_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "inactive_v6_pfxs_cnt");
  p->kp_idxs.rib_positive_mismatches_cnt_idx =                          \
    add_meta_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "rib_subtracted_pfxs_cnt");
  p->kp_idxs.rib_negative_mismatches_cnt_idx = \
    add_meta_p_metric(rt->kp, rt->metric_prefix, rt->plugin_name, p->collector_str, p->peer_str, "rib_added_pfxs_cnt");
  p->metrics_generated = 1;
}

static uint32_t
add_c_metric(timeseries_kp_t *kp, char *metric_prefix, char *plugin_name, char *sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_COLLECTOR_METRIC_FORMAT, metric_prefix, plugin_name, sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

static uint32_t
add_meta_c_metric(timeseries_kp_t *kp, char *metric_prefix, char *plugin_name, char *sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_COLLECTOR_META_METRIC_FORMAT, metric_prefix, plugin_name, sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

void 
collector_generate_metrics(routingtables_t *rt, collector_t *c)
{
  c->kp_idxs.processing_time_idx = \
    add_meta_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "processing_time");  
  c->kp_idxs.realtime_delay_idx = \
    add_meta_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "realtime_delay");  
  c->kp_idxs.valid_record_cnt_idx = \
    add_meta_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "valid_record_cnt");
  c->kp_idxs.corrupted_record_cnt_idx = \
    add_meta_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "corrupted_record_cnt");
  c->kp_idxs.empty_record_cnt_idx = \
    add_meta_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "empty_record_cnt");
  
  c->kp_idxs.status_idx = \
    add_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "status");
  c->kp_idxs.active_peers_cnt_idx = \
    add_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "active_peers_cnt");
  c->kp_idxs.active_asns_cnt_idx = \
    add_c_metric(rt->kp, rt->metric_prefix, rt->plugin_name, c->collector_str, "active_peer_asns_cnt");
}

static void
enable_peer_metrics(timeseries_kp_t *kp, perpeer_info_t *p)
{
  timeseries_kp_enable_key(kp, p->kp_idxs.status_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.active_v4_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.active_v6_pfxs_idx);

  timeseries_kp_enable_key(kp, p->kp_idxs.announcing_origin_as_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.announced_v4_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.withdrawn_v4_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.announced_v6_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.withdrawn_v6_pfxs_idx);

  timeseries_kp_enable_key(kp, p->kp_idxs.rib_messages_cnt_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.pfx_announcements_cnt_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.pfx_withdrawals_cnt_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.state_messages_cnt_idx);
              
  timeseries_kp_enable_key(kp, p->kp_idxs.inactive_v4_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.inactive_v6_pfxs_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.rib_positive_mismatches_cnt_idx);
  timeseries_kp_enable_key(kp, p->kp_idxs.rib_negative_mismatches_cnt_idx);
}

static void
disable_peer_metrics(timeseries_kp_t *kp, perpeer_info_t *p)
{
  timeseries_kp_disable_key(kp, p->kp_idxs.status_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.active_v4_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.active_v6_pfxs_idx);

  timeseries_kp_disable_key(kp, p->kp_idxs.announcing_origin_as_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.announced_v4_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.withdrawn_v4_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.announced_v6_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.withdrawn_v6_pfxs_idx);

  timeseries_kp_disable_key(kp, p->kp_idxs.rib_messages_cnt_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.pfx_announcements_cnt_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.pfx_withdrawals_cnt_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.state_messages_cnt_idx);
              
  timeseries_kp_disable_key(kp, p->kp_idxs.inactive_v4_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.inactive_v6_pfxs_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.rib_positive_mismatches_cnt_idx);
  timeseries_kp_disable_key(kp, p->kp_idxs.rib_negative_mismatches_cnt_idx);

}




static void
enable_collector_metrics(timeseries_kp_t *kp, collector_t *c)
{
  timeseries_kp_enable_key(kp, c->kp_idxs.processing_time_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.realtime_delay_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.valid_record_cnt_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.corrupted_record_cnt_idx);
    
  timeseries_kp_enable_key(kp, c->kp_idxs.empty_record_cnt_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.status_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.active_peers_cnt_idx);
  timeseries_kp_enable_key(kp, c->kp_idxs.active_asns_cnt_idx);    
}

static void
disable_collector_metrics(timeseries_kp_t *kp, collector_t *c)
{
  timeseries_kp_disable_key(kp, c->kp_idxs.processing_time_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.realtime_delay_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.valid_record_cnt_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.corrupted_record_cnt_idx);
    
  timeseries_kp_disable_key(kp, c->kp_idxs.empty_record_cnt_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.status_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.active_peers_cnt_idx);
  timeseries_kp_disable_key(kp, c->kp_idxs.active_asns_cnt_idx);    

}



void
routingtables_dump_metrics(routingtables_t *rt, uint32_t time_now)
{
  khiter_t k;
  khiter_t kp;
  collector_t *c;
  uint32_t peer_id;
  perpeer_info_t *p;
  bgpstream_peer_sig_t *sg;
  int processing_time = time_now - rt->wall_time_interval_start;
  uint32_t real_time_delay = time_now - rt->bgp_time_interval_start;
  
  /* collectors metrics */
  for (k = kh_begin(rt->collectors); k != kh_end(rt->collectors); ++k)
    {
      if (kh_exist(rt->collectors, k))
        {
          c = &kh_val(rt->collectors, k);
              
          /* compute metrics that requires peers aggregation */
          /* get all the peers that belong to the current collector */    
          for(kp = kh_begin(c->collector_peerids); kp != kh_end(c->collector_peerids); ++kp)
            {
              if(kh_exist(c->collector_peerids, kp))
                {
                  peer_id = kh_key(c->collector_peerids, kp);
                  bgpwatcher_view_iter_seek_peer(rt->iter, peer_id, BGPWATCHER_VIEW_FIELD_ALL_VALID);
                  p = bgpwatcher_view_iter_peer_get_user(rt->iter);
                  assert(p);
                  if(bgpwatcher_view_iter_peer_get_state(rt->iter) == BGPWATCHER_VIEW_FIELD_ACTIVE)
                    {
                      sg = bgpstream_peer_sig_map_get_sig(rt->peersigns, peer_id);
                      bgpstream_id_set_insert(c->active_ases, sg->peer_asnumber);                          
                    }
                }
            }

          /* set statistics here */              
          timeseries_kp_set(rt->kp, c->kp_idxs.processing_time_idx, processing_time);
          timeseries_kp_set(rt->kp, c->kp_idxs.realtime_delay_idx, real_time_delay);

          timeseries_kp_set(rt->kp, c->kp_idxs.valid_record_cnt_idx, c->valid_record_cnt);              
          timeseries_kp_set(rt->kp, c->kp_idxs.corrupted_record_cnt_idx, c->corrupted_record_cnt);              
          timeseries_kp_set(rt->kp, c->kp_idxs.empty_record_cnt_idx, c->empty_record_cnt);
              
          timeseries_kp_set(rt->kp, c->kp_idxs.status_idx, c->state);
          timeseries_kp_set(rt->kp, c->kp_idxs.active_peers_cnt_idx, c->active_peers_cnt);
          timeseries_kp_set(rt->kp, c->kp_idxs.active_asns_cnt_idx, bgpstream_id_set_size(c->active_ases));
                            
          /* flush metrics ? */
          if(c->publish_flag)
            {
              enable_collector_metrics(rt->kp, c);
            }
          else
            {
              disable_collector_metrics(rt->kp, c);
            }


          /* in all cases we have to reset the metrics */
          c->valid_record_cnt = 0;
          c->corrupted_record_cnt = 0;
          c->empty_record_cnt = 0;
           /* c->active_peers_cnt is updated by every single message */
          bgpstream_id_set_clear(c->active_ases);
          
        }
    }
  
  /* peers metrics */
  for(bgpwatcher_view_iter_first_peer(rt->iter, BGPWATCHER_VIEW_FIELD_ALL_VALID);
      bgpwatcher_view_iter_has_more_peer(rt->iter);
      bgpwatcher_view_iter_next_peer(rt->iter))
    {
      p = bgpwatcher_view_iter_peer_get_user(rt->iter);
      if(p->bgp_fsm_state != BGPSTREAM_ELEM_PEERSTATE_UNKNOWN)
        {
          /* metrics are generated the first time a peer has a not UNKNOWN state */
          if(p->metrics_generated == 0)
            {
              peer_generate_metrics(rt, p);
            }

          timeseries_kp_set(rt->kp, p->kp_idxs.status_idx, p->bgp_fsm_state);
          timeseries_kp_set(rt->kp, p->kp_idxs.active_v4_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV4,
                                                                  BGPWATCHER_VIEW_FIELD_ACTIVE));
          timeseries_kp_set(rt->kp, p->kp_idxs.inactive_v4_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV4,
                                                                  BGPWATCHER_VIEW_FIELD_INACTIVE));
          timeseries_kp_set(rt->kp, p->kp_idxs.active_v6_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV6,
                                                                  BGPWATCHER_VIEW_FIELD_ACTIVE));
          timeseries_kp_set(rt->kp, p->kp_idxs.inactive_v6_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV6,
                                                                  BGPWATCHER_VIEW_FIELD_INACTIVE));
                              
          timeseries_kp_set(rt->kp, p->kp_idxs.announcing_origin_as_idx,
                            bgpstream_id_set_size(p->announcing_ases));
          
          timeseries_kp_set(rt->kp, p->kp_idxs.announced_v4_pfxs_idx,
                            bgpstream_ipv4_pfx_set_size(p->announced_v4_pfxs));

          timeseries_kp_set(rt->kp, p->kp_idxs.withdrawn_v4_pfxs_idx,
                            bgpstream_ipv4_pfx_set_size(p->withdrawn_v4_pfxs));

          timeseries_kp_set(rt->kp, p->kp_idxs.announced_v6_pfxs_idx,
                            bgpstream_ipv6_pfx_set_size(p->announced_v6_pfxs));

          timeseries_kp_set(rt->kp, p->kp_idxs.withdrawn_v6_pfxs_idx,
                            bgpstream_ipv6_pfx_set_size(p->withdrawn_v6_pfxs));
          
          timeseries_kp_set(rt->kp, p->kp_idxs.rib_messages_cnt_idx, p->rib_messages_cnt);
          timeseries_kp_set(rt->kp, p->kp_idxs.pfx_announcements_cnt_idx, p->pfx_announcements_cnt);
          timeseries_kp_set(rt->kp, p->kp_idxs.pfx_withdrawals_cnt_idx, p->pfx_withdrawals_cnt);          
          timeseries_kp_set(rt->kp, p->kp_idxs.state_messages_cnt_idx, p->state_messages_cnt);
          timeseries_kp_set(rt->kp, p->kp_idxs.rib_positive_mismatches_cnt_idx, p->rib_positive_mismatches_cnt);
          timeseries_kp_set(rt->kp, p->kp_idxs.rib_negative_mismatches_cnt_idx, p->rib_negative_mismatches_cnt);

          enable_peer_metrics(rt->kp, p);
        }
      else
        {
           if(p->metrics_generated == 1)
            {
              disable_peer_metrics(rt->kp, p);
            }
        }
        
      /* in all cases we have to reset the metrics */
      bgpstream_id_set_clear(p->announcing_ases);
      bgpstream_ipv4_pfx_set_clear(p->announced_v4_pfxs);
      bgpstream_ipv4_pfx_set_clear(p->withdrawn_v4_pfxs);
      bgpstream_ipv6_pfx_set_clear(p->announced_v6_pfxs);
      bgpstream_ipv6_pfx_set_clear(p->withdrawn_v6_pfxs);
      p->rib_messages_cnt = 0;
      p->pfx_announcements_cnt = 0;
      p->pfx_withdrawals_cnt = 0;
      p->state_messages_cnt = 0;
      p->rib_positive_mismatches_cnt = 0;
      p->rib_negative_mismatches_cnt = 0;
    }

  timeseries_kp_flush(rt->kp, rt->bgp_time_interval_start);

}
