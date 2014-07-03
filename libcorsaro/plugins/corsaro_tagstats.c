/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtrace.h"

#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#include "corsaro_tagstats.h"

/** @file
 *
 * @brief Corsaro tag statistics plugin
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "TAGS" */
#define CORSARO_TAGSTATS_MAGIC 0x54414753

/** The name of this plugin */
#define PLUGIN_NAME "tagstats"

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_tagstats_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_TAGSTATS,                      /* id */
  CORSARO_TAGSTATS_MAGIC,                          /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_tagstats),  /* func ptrs */
  CORSARO_PLUGIN_GENERATE_TAIL,
};

  /* DELETE ME
                 #pkts-matched   #pkts-unmatched
    group-name
      tag-name
      tag-name
      tag-name

      ...

    un-grouped
      tag-name
      tag-name
      tag-name
      TOTAL

    overall
   */

typedef struct groupstat
{
  /* group corresponding to the stats */
  corsaro_tag_group_t *group;

  /* number of packets matched by this group for the current interval */
  uint64_t pkts_matched;

  /* number of packets matched by this group over all time */
  uint64_t pkts_matched_total;

  /* number of packets not matched by this group for the current interval */
  uint64_t pkts_unmatched;

  /* number of packets not matched by this group over all time */
  uint64_t pkts_unmatched_total;

} groupstat_t;

typedef struct tagstat
{
  /* tag corresponding to the stats */
  corsaro_tag_t *tag;

  /* number of packets matched by this tag for the current interval */
  uint64_t pkts_matched;

  /* number of packets matched by this tag over all time */
  uint64_t pkts_matched_total;

  /* number of packets not matched by this tag for the current interval */
  uint64_t pkts_unmatched;

  /* number of packets not matched by this tag over all time */
  uint64_t pkts_unmatched_total;

} tagstat_t;

/** Holds the state for an instance of this plugin */
struct corsaro_tagstats_state_t {
  /** Array of per-group packet counts */
  groupstat_t *groups;

  /** Number of groups that we are tracking pkt cnts for */
  int groups_cnt;

  /** Array of per-group packet counts */
  tagstat_t *tags;

  /** Number of tags that we are tracking pkt cnts for */
  int tags_cnt;

  /** Overall count of packets that we processed this interval */
  uint64_t pkt_cnt;

  /** Overall count of packets that we processed over all time */
  uint64_t pkt_cnt_total;

};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, tagstats, CORSARO_PLUGIN_ID_TAGSTATS))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_TAGSTATS))

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_tagstats_alloc(corsaro_t *corsaro)
{
  return &corsaro_tagstats_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_tagstats_probe_filename(const char *fname)
{
  /* cannot read raw tagstats files using corsaro_in */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_tagstats_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* cannot read raw tagstats files using corsaro_in */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_tagstats_init_output(corsaro_t *corsaro)
{
  int i;
  struct corsaro_tagstats_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);

  corsaro_tag_group_t **groups;
  corsaro_tag_t **tags;

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_tagstats_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		"could not malloc corsaro_tagstats_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* get all the groups that are registered */
  if((state->groups_cnt = corsaro_tag_group_get_all(corsaro, &groups)) <= 0)
    {
      fprintf(stderr, "ERROR: Could not retrieve tag groups\n");
      goto err;
    }
  if((state->groups = malloc_zero(sizeof(groupstat_t)*state->groups_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc group stats");
      goto err;
    }
  for(i=0; i<state->groups_cnt; i++)
    {
      state->groups[i].group = groups[i];
    }

  /* get all the tags that are registered */
  if((state->tags_cnt = corsaro_tag_get_all(corsaro, &tags)) <= 0)
    {
      fprintf(stderr, "ERROR: Could not retrieve tags\n");
      goto err;
    }
  if((state->tags = malloc_zero(sizeof(tagstat_t)*state->tags_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc tag stats");
      goto err;
    }
  for(i=0; i<state->tags_cnt; i++)
    {
      state->tags[i].tag = tags[i];
    }

  return 0;

 err:
  corsaro_tagstats_close_output(corsaro);
  return -1;
}

/** Implements the init_output function of the plugin API */
int corsaro_tagstats_init_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_tagstats_close_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_tagstats_close_output(corsaro_t *corsaro)
{
  int i, j;
  struct corsaro_tagstats_state_t *state = STATE(corsaro);
  groupstat_t *gs;
  tagstat_t *ts;

  if(state == NULL)
    {
      return 0;
    }

  fprintf(stdout, "OVERALL STATS\n");
  fprintf(stdout, "\t\t#matched\t#un-matched\n");
  /* write out the group stats */
  for(i=0; i<state->groups_cnt; i++)
  {
    gs = &state->groups[i];
    fprintf(stdout, "%s\t\t%"PRIu64"\t%"PRIu64"\n",
	    gs->group->name, gs->pkts_matched_total, gs->pkts_unmatched_total);
    /** now print all the tags in this group */
    for(j=0; j<gs->group->tags_cnt; j++)
      {
	ts = &state->tags[gs->group->tags[j]->id];
	assert(ts != NULL);
	fprintf(stdout, "\t%s\t%"PRIu64"\t%"PRIu64"\n",
		ts->tag->name, ts->pkts_matched_total, ts->pkts_unmatched_total);
      }
    fprintf(stdout, "\n");
  }

  fprintf(stdout, "un-grouped\n");
  for(i=0; i<state->tags_cnt; i++)
  {
    ts = &state->tags[i];
    assert(ts != NULL);
    if(ts->tag->group == NULL)
      {
	fprintf(stdout, "\t%s\t%"PRIu64"\t%"PRIu64"\n",
		ts->tag->name, ts->pkts_matched_total, ts->pkts_unmatched_total);
      }
  }
  fprintf(stdout, "\n");

  /* now, clean up */
  if(state->groups != NULL)
    {
      free(state->groups);
      state->groups = NULL;
    }
  state->groups_cnt = 0;

  if(state->tags != NULL)
    {
      free(state->tags);
      state->tags = NULL;
    }
  state->tags_cnt = 0;

  corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));

  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_tagstats_read_record(struct corsaro_in *corsaro,
			       corsaro_in_record_type_t *record_type,
			       corsaro_in_record_t *record)
{
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_tagstats_read_global_data_record(struct corsaro_in *corsaro,
			      enum corsaro_in_record_type *record_type,
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_tagstats_start_interval(corsaro_t *corsaro, corsaro_interval_t *int_start)
{
  int i;
  struct corsaro_tagstats_state_t *state = STATE(corsaro);

  /* zero out the stats */
  for(i=0; i<state->groups_cnt; i++)
    {
      state->groups[i].pkts_matched = 0;
      state->groups[i].pkts_unmatched = 0;
    }

  for(i=0; i<state->tags_cnt; i++)
    {
      state->tags[i].pkts_matched = 0;
      state->tags[i].pkts_unmatched = 0;
    }

  state->pkt_cnt = 0;
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_tagstats_end_interval(corsaro_t *corsaro, corsaro_interval_t *int_end)
{
  int i, j;
  struct corsaro_tagstats_state_t *state = STATE(corsaro);
  groupstat_t *gs;
  tagstat_t *ts;

  fprintf(stdout, "\t\t#matched\t#un-matched\n");
  /* write out the group stats */
  for(i=0; i<state->groups_cnt; i++)
  {
    gs = &state->groups[i];
    fprintf(stdout, "%s\t\t%"PRIu64"\t%"PRIu64"\n",
	    gs->group->name, gs->pkts_matched, gs->pkts_unmatched);
    /** now print all the tags in this group */
    for(j=0; j<gs->group->tags_cnt; j++)
      {
	ts = &state->tags[gs->group->tags[j]->id];
	assert(ts != NULL);
	fprintf(stdout, "\t%s\t%"PRIu64"\t%"PRIu64"\n",
		ts->tag->name, ts->pkts_matched, ts->pkts_unmatched);
      }
    fprintf(stdout, "\n");
  }

  fprintf(stdout, "un-grouped\n");
  for(i=0; i<state->tags_cnt; i++)
  {
    ts = &state->tags[i];
    assert(ts != NULL);
    if(ts->tag->group == NULL)
      {
	fprintf(stdout, "\t%s\t%"PRIu64"\t%"PRIu64"\n",
		ts->tag->name, ts->pkts_matched, ts->pkts_unmatched);
      }
  }
  fprintf(stdout, "\n\n");

  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_tagstats_process_packet(corsaro_t *corsaro,
				    corsaro_packet_t *packet)
{
  int i;
  struct corsaro_tagstats_state_t *state = STATE(corsaro);
  groupstat_t *gs;
  tagstat_t *ts;

  /* look at each group */
  for(i=0; i<state->groups_cnt; i++)
  {
    gs = &state->groups[i];
    if(corsaro_tag_group_is_match(&packet->state, gs->group) > 0)
      {
	gs->pkts_matched++;
	gs->pkts_matched_total++;
      }
    else
      {
	gs->pkts_unmatched++;
	gs->pkts_unmatched_total++;
      }
  }

  /* look at each tag */
  for(i=0; i<state->tags_cnt; i++)
  {
    ts = &state->tags[i];
    if(corsaro_tag_is_match(&packet->state, ts->tag) > 0)
      {
	ts->pkts_matched++;
	ts->pkts_matched_total++;
      }
    else
      {
	ts->pkts_unmatched++;
	ts->pkts_unmatched_total++;
      }
  }
  return 0;
}



