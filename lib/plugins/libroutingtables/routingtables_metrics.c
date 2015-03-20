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

/* <metric-prefix>.<collector-signature>.<metric-name> */
#define RT_COLLECTOR_METRIC_FORMAT "%s.%s.%s"

/* <metric-prefix>.<collector-signature>.<peer-signature>.<metric-name> */
#define RT_PEER_METRIC_FORMAT "%s.%s.%s.%s"

#define BUFFER_LEN 1024
static char metric_buffer[BUFFER_LEN];

static uint32_t
add_p_metric(timeseries_kp_t *kp, char *metric_prefix, char *c_sig, char *p_sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_PEER_METRIC_FORMAT, metric_prefix, c_sig, p_sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

void 
peer_generate_metrics(routingtables_t *rt, collector_t *c, perpeer_info_t *p)
{
  p->kp_idxs.status_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_status");
  p->kp_idxs.active_v4_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_active_v4_pfxs"); 
  p->kp_idxs.inactive_v4_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_inactive_v4_pfxs");
  p->kp_idxs.active_v6_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_active_v6_pfxs");
  p->kp_idxs.inactive_v6_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_inactive_v6_pfxs");
  p->kp_idxs.unique_origin_as_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_unique_origin_ases");
  p->kp_idxs.avg_aspath_len_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_avg_aspath_len");
  p->kp_idxs.affected_v4_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_affected_v4_pfxs");
  p->kp_idxs.affected_v6_pfxs_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_affected_v6_pfxs");
  p->kp_idxs.announced_origin_as_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_announced_origin_ases");
  p->kp_idxs.rib_messages_cnt_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_rib_messages_cnt");
  p->kp_idxs.pfx_announcements_cnt_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_pfx_announcements_cnt");
  p->kp_idxs.pfx_withdrawals_cnt_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_pfx_withdrawals_cnt");
  p->kp_idxs.state_messages_cnt_idx = \
    add_p_metric(p->kp, rt->metric_prefix, c->collector_str, p->peer_str, "peer_state_messages_cnt");
  
}

static uint32_t
add_c_metric(timeseries_kp_t *kp, char *metric_prefix, char *sig, char *metric_name)
{
  snprintf(metric_buffer, BUFFER_LEN, RT_COLLECTOR_METRIC_FORMAT, metric_prefix, sig, metric_name);
  int ret = timeseries_kp_add_key(kp, metric_buffer);
  assert(ret >= 0);
  return ret;
}

void 
collector_generate_metrics(routingtables_t *rt, collector_t *c)
{
  c->kp_idxs.processing_time_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_processing_time");  
  c->kp_idxs.realtime_delay_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_realtime_delay");  
  c->kp_idxs.status_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_status");
  c->kp_idxs.active_peers_cnt_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_active_peers_cnt");
  c->kp_idxs.unique_v4_pfxs_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_unique_v4_pfxs");
  c->kp_idxs.unique_v6_pfxs_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_unique_v6_pfxs");
  c->kp_idxs.unique_origin_as_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_unique_origin_ases");
  c->kp_idxs.affected_v4_pfxs_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_affected_v4_pfxs");
  c->kp_idxs.affected_v6_pfxs_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_affected_v6_pfxs");
  c->kp_idxs.announced_origin_as_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_announced_origin_ases");
  c->kp_idxs.valid_record_cnt_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_valid_record_cnt");
  c->kp_idxs.corrupted_record_cnt_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_corrupted_record_cnt");
  c->kp_idxs.empty_record_cnt_idx = \
    add_c_metric(c->kp, rt->metric_prefix, c->collector_str, "collector_empty_record_cnt");  
}

int
routingtables_dump_metrics(routingtables_t *rt, uint32_t time_now)
{
  khiter_t k;
  collector_t *c;
  perpeer_info_t *p;
  
  int processing_time = time_now - rt->wall_time_interval_start;
  uint32_t real_time_delay = time_now - rt->bgp_time_interval_start;
  
  /* collectors metrics */
  for (k = kh_begin(rt->collectors); k != kh_end(rt->collectors); ++k)
    {
      if (kh_exist(rt->collectors, k))
        {
          c = &kh_val(rt->collectors, k);
          if(c->state != ROUTINGTABLES_COLLECTOR_STATE_UNKNOWN)
            {
              timeseries_kp_set(c->kp, c->kp_idxs.processing_time_idx, processing_time);
              timeseries_kp_set(c->kp, c->kp_idxs.realtime_delay_idx, real_time_delay);
              timeseries_kp_set(c->kp, c->kp_idxs.status_idx, c->state);
              timeseries_kp_set(c->kp, c->kp_idxs.active_peers_cnt_idx, c->active_peers_cnt);
              c->active_peers_cnt = 0;
              
              /* @todo collect the remaining metrics and set their value here */

              timeseries_kp_set(c->kp, c->kp_idxs.valid_record_cnt_idx, c->valid_record_cnt);
              c->valid_record_cnt = 0;
              timeseries_kp_set(c->kp, c->kp_idxs.corrupted_record_cnt_idx, c->corrupted_record_cnt);
              c->corrupted_record_cnt = 0;
              timeseries_kp_set(c->kp, c->kp_idxs.empty_record_cnt_idx, c->empty_record_cnt);
              c->empty_record_cnt = 0;
              
              /* flush metrics */
              timeseries_kp_flush(c->kp, rt->bgp_time_interval_start);
            }
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
          timeseries_kp_set(p->kp, p->kp_idxs.status_idx, p->bgp_fsm_state);
          timeseries_kp_set(p->kp, p->kp_idxs.active_v4_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV4,
                                                                  BGPWATCHER_VIEW_FIELD_ACTIVE));
          timeseries_kp_set(p->kp, p->kp_idxs.inactive_v4_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV4,
                                                                  BGPWATCHER_VIEW_FIELD_INACTIVE));
          timeseries_kp_set(p->kp, p->kp_idxs.active_v6_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV6,
                                                                  BGPWATCHER_VIEW_FIELD_ACTIVE));
          timeseries_kp_set(p->kp, p->kp_idxs.inactive_v6_pfxs_idx,
                            bgpwatcher_view_iter_peer_get_pfx_cnt(rt->iter,
                                                                  BGPSTREAM_ADDR_VERSION_IPV6,
                                                                  BGPWATCHER_VIEW_FIELD_INACTIVE));
          /* @todo collect the remaining metrics and set their value here */
          
          
          timeseries_kp_set(p->kp, p->kp_idxs.pfx_announcements_cnt_idx, p->pfx_announcements_cnt);
          p->pfx_announcements_cnt = 0;
          timeseries_kp_set(p->kp, p->kp_idxs.pfx_withdrawals_cnt_idx, p->pfx_withdrawals_cnt);
          p->pfx_withdrawals_cnt = 0;
          
          /* @todo collect the remaining metrics and set their value here */

          timeseries_kp_set(p->kp, p->kp_idxs.rib_messages_cnt_idx, p->rib_messages_cnt);
          p->rib_messages_cnt = 0;
          timeseries_kp_set(p->kp, p->kp_idxs.pfx_announcements_cnt_idx, p->pfx_announcements_cnt);
          p->pfx_announcements_cnt = 0;
          timeseries_kp_set(p->kp, p->kp_idxs.pfx_withdrawals_cnt_idx, p->pfx_withdrawals_cnt);
          p->pfx_withdrawals_cnt = 0;
          timeseries_kp_set(p->kp, p->kp_idxs.state_messages_cnt_idx, p->state_messages_cnt);
          p->state_messages_cnt = 0;
          
          timeseries_kp_flush(p->kp, rt->bgp_time_interval_start);          
        }            
    }
  
  return 0;
}
