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

#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "libtrace.h"
#ifdef HAVE_LIBPACKETDUMP
#include "libpacketdump.h"
#endif

#include "khash.h"
#include "ksort.h"
#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#include "corsaro_dos.h"

/** @file
 *
 * @brief Corsaro new_rsdos plugin implementation
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "EDOS" */
#define CORSARO_DOS_MAGIC 0x45444F53

/** The name of this plugin */
#define PLUGIN_NAME "dos"

/** The old name of this plugin
 *
 * Because the original files created with the dos plugin do not have
 * a magic number, we rely on the filename check. This is pretty fragile,
 * but it's all we have right now
 */
#define PLUGIN_NAME_DEPRECATED "edgar_dos"

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_dos_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_DOS,                         /* id */
  CORSARO_DOS_MAGIC,                                           /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_dos),  /* func ptrs */
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** The interval that this plugin would like to dump at
 *
 * NOTE THIS WELL!
 * There is a known issue with how this plugin computes which corsaro intervals
 * to dump on. IF the corsaro interval is set to longer than the time in the
 * trace, and this is longer than the dos interval, it WILL NOT dump anything
 *
 * To fix this, we should implement some method for plugins to choose an interval
 * which corsaro will call them on its closest interval.
 *
 */
#define CORSARO_DOS_INTERVAL                   300

/** The length of time after which an inactive attack vector is expired */
#define CORSARO_DOS_VECTOR_TIMEOUT             CORSARO_DOS_INTERVAL

/** The minimum number of packets before a vector can be an attack */
#define CORSARO_DOS_ATTACK_VECTOR_MIN_PACKETS  25

/** The minimum number of seconds before a vector can be an attack */
#define CORSARO_DOS_ATTACK_VECTOR_MIN_DURATION 60

/** The minimum packet rate before a vector can be an attack */
#define CORSARO_DOS_ATTACK_VECTOR_MIN_PPM      30

/** The length (in bytes) of an attack vector record */
#define CORSARO_DOS_ATTACK_VECTOR_BYTECNT      (4+4+4+4+4+8+4+8+4+8+4+4+4+4+4)

/** The length of the pps sliding window in seconds */
#define CORSARO_DOS_PPM_WINDOW_SIZE      60

/** The amount to slide the window by in seconds */
#define CORSARO_DOS_PPM_WINDOW_PRECISION 10

/** The number of buckets */
#define CORSARO_DOS_PPS_BUCKET_CNT       (CORSARO_DOS_PPM_WINDOW_SIZE/	\
					CORSARO_DOS_PPM_WINDOW_PRECISION)

/** Initialize the hash types needed to hold maps in vectors
 *
 * The convention is a 4 digit name, where the first two digits indicate
 * the length of the key, and the last two indicate the length of the value
 * e.g. 3264 means 32 bit integer keys with 64bit integer values
 */
KHASH_SET_INIT_INT(32xx)

/** State for the sliding packet rate algorithm */
typedef struct ppm_window
{
  /** Time of the bottom of the current first window */
  uint32_t window_start;
  /** The number of packets in each bucket */
  uint64_t buckets[CORSARO_DOS_PPS_BUCKET_CNT];
  /** The bucket that packets are currently being added to */
  uint8_t current_bucket;
  /** The maximum packet rate observed thus far */
  uint64_t max_ppm;
} ppm_window_t;

/** A record for a potential attack vector 
 *
 * All values are in HOST byte order
 */
typedef struct attack_vector
{
  /** A copy of the packet that caused the vector to be created 
   *
   * Can be reconstituted into a libtrace packet
   */
  uint8_t *initial_packet;

  /** Length of the initial packet (in bytes) */
  uint32_t initial_packet_len;

  /** The IP address of the alleged attacker */
  uint32_t attacker_ip;

  /** The IP address of the host which responded to the attack */
  uint32_t responder_ip;

  /** The IP address of the alleged target of the attack */
  uint32_t target_ip;

  /** The number of packets that comprise this vector */
  uint64_t packet_cnt;

  /** The number of packets added to this vector in the current interval */
  uint32_t interval_packet_cnt;

  /** The number of bytes that comprise this vector */
  uint64_t byte_cnt;

  /** The number of bytes added to this vector in the current interval */
  uint32_t interval_byte_cnt;

  /** The sliding window packet rate state */
  ppm_window_t   ppm_window;

  /** The time of the initial packet */
  struct timeval start_time;

  /** The time of the last packet */
  struct timeval latest_time;

  /** Map of all IP addresses the alleged attack has originated from */
  kh_32xx_t *attack_ip_hash;

  /** Map of all ports that alleged attack packets have originated from */
  kh_32xx_t *attack_port_hash;

  /** Map of all ports that alleged attack packets were directed to */
  kh_32xx_t *target_port_hash;

  /** Number of IP addresses that have been used to send packets */
  uint32_t attack_ip_cnt;

} attack_vector_t;

/* need to create an attack_vector_in structure when we write the reading 
   stuff */

/** Create an attack vector object 
 *
 * @param corsaro      The corsaro object associated with the vector 
 * @return an empty attack vector object
 */
static attack_vector_t *attack_vector_init(corsaro_t *corsaro)
{
  attack_vector_t *av = NULL;
  if((av = malloc_zero(sizeof(attack_vector_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc memory for attack vector");
      return NULL;
    }

  av->attack_ip_hash = kh_init(32xx);
  assert(av->attack_ip_hash != NULL);
  
  av->attack_port_hash = kh_init(32xx);
  assert(av->attack_port_hash != NULL);

  av->target_port_hash = kh_init(32xx);
  assert(av->target_port_hash != NULL);

  return av;
}

/** Free the memory allocated to an attack vector object 
 *
 * @param av        The attack vector to be freed
 */
static void attack_vector_free(attack_vector_t *av)
{
  if(av == NULL)
    {
      return;
    }

  if(av->initial_packet != NULL)
    {
      /*trace_destroy_packet(av->initial_packet);*/
      free(av->initial_packet);
    }

  if(av->attack_ip_hash != NULL)
    {
      kh_destroy(32xx, av->attack_ip_hash);
    }
  if(av->attack_port_hash != NULL)
    {
      kh_destroy(32xx, av->attack_port_hash);
    }
  if(av->target_port_hash != NULL)
    {
      kh_destroy(32xx, av->target_port_hash);
    }

  free(av);
  return;
}

/** Reset the per-interval counters in an attack vector 
 *
 * @param av        The attack vector to be reset
 */
static void attack_vector_reset(attack_vector_t *av)
{
  assert(av != NULL);

  av->interval_packet_cnt = 0;
  av->interval_byte_cnt = 0;
  av->attack_ip_cnt = kh_size(av->attack_ip_hash);
}

/** Compare two attack vectors for equality */
#define attack_vector_hash_equal(a, b) (				\
					(a)->target_ip == (b)->target_ip \
									)

/** Hash an attack vector 
 *
 * @param av         The attack vector to be hashed 
 */
static inline khint32_t attack_vector_hash_func(attack_vector_t *av)
{
  return (khint32_t)av->target_ip*59;
}

/** Initialize the hash functions and datatypes */
KHASH_INIT(av, attack_vector_t*, char, 0, 
	   attack_vector_hash_func, attack_vector_hash_equal);

/** Holds the state for an instance of this plugin */
struct corsaro_dos_state_t {
  /** The time that corsaro first asked us to end an interval */
  uint32_t first_interval;
  /** The number of packets for which the inner ICMP IP does not match the
      outer IP one */
  uint16_t number_mismatched_packets;
  /** The map of potential attack vectors */
  khash_t(av) *attack_hash;
  /** The outfile for the plugin */
  corsaro_file_t *outfile;
};

/** Holds the state for an instance of this plugin (when reading data) */
struct corsaro_dos_in_state_t {
  /** The expected type of the next record in the file */
  corsaro_in_record_type_t expected_type;
  /** The number of elements in the current distribution */
  int vector_total;
  /** The number of elements already read in the current distribution */
  int vector_cnt;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, dos,CORSARO_PLUGIN_ID_DOS))
/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE_IN(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, dos_in,			\
			CORSARO_PLUGIN_ID_DOS))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_DOS))

/** Check if a vector has had a packet added to it recently 
 *
 * @param vector        The attack vector to check
 * @param ts            The current trace time
 */
static int attack_vector_is_expired(attack_vector_t *vector, 
				    uint32_t time)
{
  if(vector->latest_time.tv_sec + CORSARO_DOS_VECTOR_TIMEOUT < time)
    {
      return 1;
    }
  return 0;
}

/** Update the max ppm value given the current window values
 * @param ppm_window     The packet rate window to update
 */
static void attack_vector_update_ppm(ppm_window_t *ppm_window)
{
  int i;
  uint64_t this_ppm;

  /* calculate the ppm for the current window in the buckets */
  this_ppm = 0;
  for(i = 0; i < CORSARO_DOS_PPS_BUCKET_CNT; i++)
    {
      this_ppm += ppm_window->buckets[i];
    }
  if(this_ppm > ppm_window->max_ppm)
    {
      ppm_window->max_ppm = this_ppm;
    }
}

/** Update the packet rate window
 *
 * @param vector      The attack vector to update
 * @param tv          The time the packet arrived at
 * 
 * PPS Sliding Window
 *
 * In order to properly handle short-duration spikes in the PPS rate, we use
 * a sliding window for each attack vector.
 *
 * When a vector is first created, the initial packet time is used as the start
 * of the window. Thereafter, packets added to the vector are used to update
 * a sliding window of values.
 *
 * Updating the window:
 * When a packet is received, it is first checked to determine whether the
 * window must be moved. It will be moved if this packet arrived more than 
 * PPM_WINDOW_SIZE seconds after the ppm_window.window_size value. If this
 * is the case, the current bucket pointer is moved on one%PPS_BUCKET_CNT
 * this now has it pointing at the old start of the window, this value
 * is then zeroed and ppm_window.window_start is incremented by 
 * PPM_WINDOW_PRECISION. This move is repeated until the new value falls
 * into the bucket pointed to by the current_bucket pointer 
 * I.e. Its time, x, satisfies
 * (window_start+(PPM_WINDOW_PRECISION * (PPS_BUCKET_CNT-1))) <= x < 
 *         (window_start+(PPM_WINDOW_PRECISION * PPS_BUCKET_CNT))
 * The value of buckets[current_bucket] is then incremented by one
 *
 * Computing the PPS rate:
 * The maximum PPS rate for an attack vector is the maximum PPS rate across
 * all of the windows observed. As such, every time the window is moved, we
 * update the max_ppm value by summing the value in each bucket
 *
 * For example:
 * window_start: 1320969600
 * max_ppm: 56
 * current_bucket: 5
 * bucket | value
 * 0      | 12
 * 1      | 2
 * 2      | 3
 * 3      | 6
 * 4      | 8
 * 5      | 1
 *
 * the current bucket of 5 covers times from 
 * (1320969600+(10*(6-1))) up to, but not including (1320969600+(10*6))
 * or, 1320969650 <= x < 1320969660
 *  
 * we receive a packet at 1320969665 which is 65 seconds after 1320969600
 * this means we will need to move the window,
 * we first compute the ppm for the window that just ended by summing all 
 * buckets (12+2+3+6+8+1) is 32, not higher than the max so nothing is done
 * we then advance the window by setting the current bucket to (5+1)%6, or, 0
 * and then setting the value in this bucket to 0.
 * Because this example had the next packet arrive in the very next window, 
 * the window only needs to be advanced once, if it had been later, the window
 * would have been advanced multiple times until the packet fell into the last
 * window.
 *
 * This implementation will work fairly efficiently if packets tend to arrive
 * close together, if they are spaced by long amounts of time, it will be less
 * efficient. We can help this by calculating how many times the window will
 * need to be advanced and then zeroing the appropriate number of buckets
 * before calculating the new ppm rate and updating the current window pointer.
 * The formula for this calculation is
 * delta: ((new_time) - (window_start+(PPM_WINDOW_SIZE)))
 * buckets to zero: min(PPS_BUCKET_CNT, (delta/PPM_WINDOW_PRECISION)+1))
 *     (assuming delta is > 0)
 * 
 * In our previous example, if the new packet had arrived at 1320969700,
 * the delta would be (1320969700-(1320969600+60)) = 40
 * we would need to zero (min(6, (40/10)+1)) = 5 buckets
 *
 * the truth is in the code, see attack_vector_update_ppm_window
 */
static void attack_vector_update_ppm_window(attack_vector_t *vector, 
					   struct timeval tv)
{
  int bucket_offset;
  int i;

  ppm_window_t *ppm_window = &vector->ppm_window;

  bucket_offset = (tv.tv_sec-ppm_window->window_start)/
    CORSARO_DOS_PPM_WINDOW_PRECISION;

  /* this packet is outside of the current bucket */
  if(bucket_offset > 0)
    {
      attack_vector_update_ppm(ppm_window);
 
      /* zero out the first n buckets in the window */
      for(i = 0; i < bucket_offset && i < 6; i++)
	{
	  ppm_window->current_bucket = 
	    (ppm_window->current_bucket+1) % CORSARO_DOS_PPS_BUCKET_CNT;
	  ppm_window->buckets[ppm_window->current_bucket] = 0;
	} 
      /* move the start of the window to the end of the zeroed buckets */
      ppm_window->window_start += bucket_offset*
	CORSARO_DOS_PPM_WINDOW_PRECISION;

    }

  /* add this packet to current bucket */
  ppm_window->buckets[ppm_window->current_bucket]++;

  return;
}

/** Determine whether a vector is indeed an attack vector
 * 
 * @param corsaro         The corsaro object associated with the vector
 * @param vector        The vector to check
 * @param ts            The current trace time
 * @return 1 if the vector is an attack, 0 if non-attack, -1 if an error occurs
 */
static int attack_vector_is_attack(corsaro_t *corsaro,
				   attack_vector_t *vector, 
				   uint32_t time)
{
  struct timeval duration;
  uint64_t ppm;

  if(vector->packet_cnt < CORSARO_DOS_ATTACK_VECTOR_MIN_PACKETS)
    {
      /* not enough packets */
      return 0;
    }

  if(timeval_subtract(&duration, &vector->latest_time, 
		      &vector->start_time) == 1)
    {
      corsaro_log(__func__, corsaro, "last packet seen before first packet!");
      return -1;
    }
  if(duration.tv_sec < CORSARO_DOS_ATTACK_VECTOR_MIN_DURATION)
    {
      /* not long enough */
      return 0;
    }

  attack_vector_update_ppm(&vector->ppm_window);
  ppm = vector->ppm_window.max_ppm;

  if(ppm < CORSARO_DOS_ATTACK_VECTOR_MIN_PPM)
    {
      /* not high enough velocity */
      return 0;
    }

  return 1;
}

/** Dump the given vector to the plugin output file in ASCII
 *
 * @param corsaro       The corsaro object associated with the vector
 * @param vector      The vector to dump
 * @return 0 if the vector is dumped successfully, -1 if an error occurs
 */
static int ascii_dump(corsaro_t *corsaro, attack_vector_t *vector)
{
  uint32_t tmp;
  char t_ip[16];

  tmp = htonl(vector->target_ip);
  inet_ntop(AF_INET,&tmp, &t_ip[0], 16);

  corsaro_file_printf(corsaro, STATE(corsaro)->outfile, 
	  "%s"
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32".%06"PRIu32
	  ",%"PRIu32".%06"PRIu32
	  "\n",
	  t_ip,
	  kh_size(vector->attack_ip_hash),
	  kh_size(vector->attack_ip_hash)-vector->attack_ip_cnt, 
	  kh_size(vector->attack_port_hash), 
	  kh_size(vector->target_port_hash),
	  vector->packet_cnt,
	  vector->interval_packet_cnt,
	  vector->byte_cnt,
	  vector->interval_byte_cnt,
	  vector->ppm_window.max_ppm,
	  (uint32_t)vector->start_time.tv_sec, 
	  (uint32_t)vector->start_time.tv_usec,
	  (uint32_t)vector->latest_time.tv_sec, 
	  (uint32_t)vector->latest_time.tv_usec);
  return 0;
}

/** Dump the given vector to the plugin output file in binary
 *
 * @param corsaro       The corsaro object associated with the vector
 * @param vector      The vector to dump
 * @return 0 if the vector is dumped successfully, -1 if an error occurs
 */
static int binary_dump(corsaro_t *corsaro, attack_vector_t *vector)
{
  uint8_t av_bytes[CORSARO_DOS_ATTACK_VECTOR_BYTECNT];
  uint8_t *ptr = &av_bytes[0];

  /*
  uint8_t *pkt_buf = NULL;
  libtrace_linktype_t linktype;
  uint32_t pkt_length;
  */

  /* dump the attack vector details */

  bytes_htonl(ptr, vector->target_ip);
  ptr+=4;

  bytes_htonl(ptr, kh_size(vector->attack_ip_hash));
  ptr+=4;

  bytes_htonl(ptr, kh_size(vector->attack_ip_hash)-vector->attack_ip_cnt);
  ptr+=4;

  bytes_htonl(ptr, kh_size(vector->attack_port_hash));
  ptr+=4;

  bytes_htonl(ptr, kh_size(vector->target_port_hash));
  ptr+=4;

  bytes_htonll(ptr, vector->packet_cnt);
  ptr+=8;

  bytes_htonl(ptr, vector->interval_packet_cnt);
  ptr+=4;

  bytes_htonll(ptr, vector->byte_cnt);
  ptr+=8;

  bytes_htonl(ptr, vector->interval_byte_cnt);
  ptr+=4;

  bytes_htonll(ptr, vector->ppm_window.max_ppm);
  ptr+=8;
  
  bytes_htonl(ptr, vector->start_time.tv_sec);
  ptr+=4;

  bytes_htonl(ptr, vector->start_time.tv_usec);
  ptr+=4;

  bytes_htonl(ptr, vector->latest_time.tv_sec);
  ptr+=4;

  bytes_htonl(ptr, vector->latest_time.tv_usec);
  ptr+=4;

  /* dump the initial packet using trace_get_packet_buffer */
  /*
  if((pkt_buf = trace_get_packet_buffer(vector->initial_packet,
					&linktype, NULL)) == NULL ||
     (pkt_length = trace_get_capture_length(vector->initial_packet)) == 0)
    {
      corsaro_log(__func__, "could not get packet buffer");
      return -1;
    }
  */

  /* add the size of the packet to the byte array before we dump it */
  bytes_htonl(ptr, vector->initial_packet_len);

  if(corsaro_file_write(corsaro, STATE(corsaro)->outfile, &av_bytes[0], 
		      CORSARO_DOS_ATTACK_VECTOR_BYTECNT) != 
     CORSARO_DOS_ATTACK_VECTOR_BYTECNT)
    {
      corsaro_log(__func__, corsaro, "could not dump vector byte array to file");
      return -1;
    }

  if(corsaro_file_write(corsaro, STATE(corsaro)->outfile, vector->initial_packet, 
		      vector->initial_packet_len) != 
     vector->initial_packet_len)
    {
      corsaro_log(__func__, corsaro, "could not dump packet to file");
      return -1;
    }
  
  return 0;
}

static int read_header(corsaro_in_t *corsaro, 
		       corsaro_in_record_type_t *record_type,
		       corsaro_in_record_t *record)
{
  off_t bytes_read;

  if((bytes_read =
      corsaro_io_read_bytes(corsaro, record, 
			  sizeof(corsaro_dos_header_t))) != 
     sizeof(corsaro_dos_header_t))
    {
      corsaro_log_in(__func__, corsaro, "failed to read dos header from file");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  ((corsaro_dos_header_t *)record->buffer)->attack_vector_cnt = 
  ntohl(((corsaro_dos_header_t *)record->buffer)->attack_vector_cnt);

  assert(bytes_read == sizeof(corsaro_dos_header_t));

  *record_type = CORSARO_IN_RECORD_TYPE_DOS_HEADER;
  STATE_IN(corsaro)->vector_total = ((corsaro_dos_header_t *)
				  record->buffer)->attack_vector_cnt;

  STATE_IN(corsaro)->expected_type = (STATE_IN(corsaro)->vector_total == 0) ?
    CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END :
    CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR;
  
  return bytes_read;
}

static int validate_attack_vector(corsaro_dos_attack_vector_in_t *av)
{
  /* short-circuit if the packet is empty */
  if(av->initial_packet_len == 0)
    {
      return 0;
    }

  /* we need to byte swap */
  av->target_ip = ntohl(av->target_ip);
  av->attacker_ip_cnt = ntohl(av->attacker_ip_cnt);
  av->interval_attacker_ip_cnt = ntohl(av->interval_attacker_ip_cnt);
  av->attack_port_cnt = ntohl(av->attack_port_cnt);
  av->target_port_cnt = ntohl(av->target_port_cnt);
  av->packet_cnt = ntohll(av->packet_cnt);
  av->interval_packet_cnt = ntohl(av->interval_packet_cnt);
  av->byte_cnt = ntohll(av->byte_cnt);
  av->interval_byte_cnt = ntohl(av->interval_byte_cnt);
  av->max_ppm = ntohll(av->max_ppm);
  av->start_time_sec = ntohl(av->start_time_sec);
  av->start_time_usec = ntohl(av->start_time_usec);
  av->latest_time_sec = ntohl(av->latest_time_sec);
  av->latest_time_usec = ntohl(av->latest_time_usec);
  av->initial_packet_len = ntohl(av->initial_packet_len);

  /* NULL it just in case */
  av->initial_packet = NULL;
  return 1;
}

static int read_attack_vector(corsaro_in_t *corsaro, 
			      corsaro_in_record_type_t *record_type,
			      corsaro_in_record_t *record)
{
  off_t bytes_read;
  
  /* the number of bytes that should be read after the first read */
  /* this is the size of the attack vector less the size of the pointer */
  off_t bsbread = sizeof(corsaro_dos_attack_vector_in_t)
    -sizeof(uint8_t*);

  corsaro_dos_attack_vector_in_t *av = NULL;

  /* read the attack vector in record, but not the pointer to the packet
     we will need to find the length before we read it in */

  if((bytes_read = corsaro_io_read_bytes(corsaro, record, bsbread)) != bsbread)
    {
      corsaro_log_in(__func__, corsaro, 
		     "failed to read dos attack vector from file");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  av = (corsaro_dos_attack_vector_in_t *)record->buffer;

  if(validate_attack_vector(av) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate attack vector");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  /* now read the packet into the buffer right after the attack vector */
  if((bytes_read +=
      corsaro_io_read_bytes_offset(corsaro, record, 
				   sizeof(corsaro_dos_attack_vector_in_t),
				   av->initial_packet_len))
     != (bsbread += av->initial_packet_len))
    {
      corsaro_log_in(__func__, corsaro,
		     "failed to read initial packet from file");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  /* update the pointer */
  av->initial_packet = record->buffer+sizeof(corsaro_dos_attack_vector_in_t);

  *record_type = CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR;

  if(++(STATE_IN(corsaro)->vector_cnt) == STATE_IN(corsaro)->vector_total)
    {
      STATE_IN(corsaro)->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END;
      STATE_IN(corsaro)->vector_total = 0;
      STATE_IN(corsaro)->vector_cnt = 0;
    }

  assert(bytes_read == sizeof(corsaro_dos_attack_vector_in_t)-sizeof(uint8_t*)
	 +av->initial_packet_len);

  return bytes_read;
}

static int validate_global_header(corsaro_dos_global_header_t *g)
{
  g->mismatched_pkt_cnt = ntohl(g->mismatched_pkt_cnt);
  g->attack_vector_cnt = ntohl(g->attack_vector_cnt);
  g->non_attack_vector_cnt = ntohl(g->non_attack_vector_cnt);

  return 1;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

corsaro_plugin_t *corsaro_dos_alloc(corsaro_t *corsaro)
{
  return &corsaro_dos_plugin;
}

int corsaro_dos_probe_filename(const char *fname)
{
  /* look for 'corsaro_dos' in the name */
  if(corsaro_plugin_probe_filename(fname, &corsaro_dos_plugin) == 0)
    {
      if(strstr(fname, PLUGIN_NAME_DEPRECATED) != NULL)
	{
	  return 1;
	}
    }
  else
    {
      return 1;
    }
  return 0;
}

/**
 * @todo add a magic number and make it backwards compatible
 */
int corsaro_dos_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* unfortunately we cant detect this in corsaro 0.6 files.
     alistair was an idiot and forgot to write an magic number for the
     DOS plugin. */
  return -1;
}

/**
 * @todo dump full corsaro headers
 */
int corsaro_dos_init_output(corsaro_t *corsaro)
{
  struct corsaro_dos_state_t *state;
  /* retrieve a pointer to the plugin struct with our name and id */
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  assert(plugin != NULL);

  /* 
   * allocate memory for the state structure which will hold a pointer to the 
   * output file and other statistics 
   */
  if((state = malloc_zero(sizeof(struct corsaro_dos_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		"could not malloc corsaro_dos_state_t");
      goto err;
    }
  /* 
   * register the state structure with the plugin manager
   * this associates it with our plugin id so it can be retrieved later
   */
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* init the hash table for attack vectors */
  state->attack_hash = kh_init(av);

  return 0;

 err:
  corsaro_dos_close_output(corsaro);
  return -1;
}

int corsaro_dos_init_input(corsaro_in_t *corsaro)
{
  struct corsaro_dos_in_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_dos_in_state_t))) 
     == NULL)
    {
      corsaro_log_in(__func__, corsaro, 
		"could not malloc corsaro_dos_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* we initially expect an corsaro interval record */
  state->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;

  /* don't set the vector_cnt until we actually see a header record */
  
  return 0;

 err:
  corsaro_dos_close_input(corsaro);
  return -1;
}

int corsaro_dos_close_input(corsaro_in_t *corsaro)
{
  struct corsaro_dos_in_state_t *state = STATE_IN(corsaro);

  if(state != NULL)
    {
      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

int corsaro_dos_close_output(corsaro_t *corsaro)
{
  struct corsaro_dos_state_t *state = STATE(corsaro);

  if(state != NULL)
    {
      if(state->attack_hash != NULL)
	{
	  kh_free(av, state->attack_hash, &attack_vector_free);
	  kh_destroy(av, state->attack_hash);
	  state->attack_hash = NULL;
	}

      if(state->outfile != NULL)
	{
	  corsaro_file_close(corsaro, state->outfile);
	  state->outfile = NULL;
	}
      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

off_t corsaro_dos_read_record(struct corsaro_in *corsaro, 
			  corsaro_in_record_type_t *record_type, 
			  corsaro_in_record_t *record)
{
  struct corsaro_dos_in_state_t *state = STATE_IN(corsaro);

  off_t bytes_read = -1;
  
  /* this code is adapted from corsaro_flowtuple.c */
  /* we have 5 different types of records that could be in this file */
  switch(state->expected_type)
    {
    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_start(corsaro, corsaro->file, 
						  record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  state->expected_type = CORSARO_IN_RECORD_TYPE_DOS_HEADER;
	}
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_HEADER:
      /* we'll handle this one */
      bytes_read = read_header(corsaro, record_type, record);
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR:
      /* we'll handle this too */
      bytes_read = read_attack_vector(corsaro, record_type, record);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_end(corsaro, corsaro->file,
						record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  state->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;
	}
      break;

    default:
      corsaro_log_in(__func__, corsaro, "invalid expected record type");
    }

  return bytes_read;
}

off_t corsaro_dos_read_global_data_record(struct corsaro_in *corsaro, 
			      enum corsaro_in_record_type *record_type, 
			      struct corsaro_in_record *record)
{
  off_t bytes_read;

  if((bytes_read = corsaro_io_read_bytes(corsaro, record, 
				   sizeof(corsaro_dos_global_header_t))) != 
     sizeof(corsaro_dos_global_header_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  if(validate_global_header((corsaro_dos_global_header_t *)record->buffer) 
     != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate global header");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  *record_type = CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER;

  assert(bytes_read == sizeof(corsaro_dos_global_header_t));

  return bytes_read;
}

int corsaro_dos_start_interval(corsaro_t *corsaro, corsaro_interval_t *int_start)
{
  /* open the output file if it has been closed */
  if(STATE(corsaro)->outfile == NULL && 
     (STATE(corsaro)->outfile = 
      corsaro_io_prepare_file(corsaro, PLUGIN(corsaro)->name,
			      int_start)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not open %s output file", 
		  PLUGIN(corsaro)->name);
      return -1;
    }

  if(STATE(corsaro)->first_interval == 0)
    {
      /* -1 to simulate the end of the 'previous' interval */
      STATE(corsaro)->first_interval = int_start->time-1;
    }

  return 0;
}

int corsaro_dos_end_interval(corsaro_t *corsaro, corsaro_interval_t *int_end)
{
  int this_interval = int_end->time-STATE(corsaro)->first_interval;

  khiter_t i;
  attack_vector_t *vector;
  attack_vector_t **attack_arr = NULL;
  int attack_arr_cnt = 0;

  uint8_t gbuf[12];
  uint8_t cntbuf[4];

  if(this_interval < CORSARO_DOS_INTERVAL)
    {
      /* we haven't run for long enough to dump */
      return 0;
    }
  else
    {
      /* we either have hit exactly the right amount of time,
	 or we have gone for too long, dump now and reset the counter */
      STATE(corsaro)->first_interval = int_end->time;
      /* fall through and continue to dump */
    }

  /* this is an interval we care about */

  /* malloc an array big enough to hold the entire hash even though we wont
     need it to be that big */
  if((attack_arr = 
      malloc(sizeof(attack_vector_t *)*
	     kh_size(STATE(corsaro)->attack_hash))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "could not malloc array for attack vectors");
      return -1;
    }

  /* classify the flows and dump the attack ones */

  for(i = kh_begin(STATE(corsaro)->attack_hash); 
      i != kh_end(STATE(corsaro)->attack_hash); ++i)			
    {	
      if(kh_exist(STATE(corsaro)->attack_hash, i))
	{
	  vector = kh_key(STATE(corsaro)->attack_hash, i);

	  if(attack_vector_is_expired(vector, int_end->time) != 0)
	    {
	      kh_del(av, STATE(corsaro)->attack_hash, i);
	      attack_vector_free(vector);
	      vector = NULL;
	    }
	  else if(attack_vector_is_attack(corsaro, vector, int_end->time) != 0)
	    {
	      /* this is an attack */
	      /* add it to the attack array so we can know how many
		 before we dump it */
	      attack_arr[attack_arr_cnt] = vector;
	      attack_arr_cnt++;
	    }
	  else
	    {
	      attack_vector_reset(vector);
	    }
	}
    }

  corsaro_io_write_interval_start(corsaro, STATE(corsaro)->outfile, 
				  &corsaro->interval_start);
  corsaro_io_write_plugin_start(corsaro, corsaro->global_file, PLUGIN(corsaro));
  if(CORSARO_FILE_MODE(STATE(corsaro)->outfile) == CORSARO_FILE_MODE_ASCII)
    {
      /* global stats */
      /* dump the number of mismatched packets and vectors */
      corsaro_file_printf(corsaro, corsaro->global_file, 
			"mismatch: %"PRIu32"\n"
			"attack_vectors: %"PRIu32"\n"
			"non-attack_vectors: %"PRIu32"\n", 
			STATE(corsaro)->number_mismatched_packets,
			attack_arr_cnt,
			kh_size(STATE(corsaro)->attack_hash)-attack_arr_cnt);

      /* dump the number of vectors */
      corsaro_file_printf(corsaro, STATE(corsaro)->outfile, "%"PRIu32"\n", 
			attack_arr_cnt);
      /* dump the vectors */
      for(i = 0; i < attack_arr_cnt; i++)
	{
	  if(ascii_dump(corsaro, attack_arr[i]) != 0)
	    {
	      corsaro_log(__func__, corsaro, "could not dump hash");
	      return -1;
	    }
	  /* reset the interval stats */
	  attack_vector_reset(attack_arr[i]);
	}
    }
  else if(CORSARO_FILE_MODE(STATE(corsaro)->outfile) == CORSARO_FILE_MODE_BINARY)
      {
	/* global stats */
	bytes_htonl(&gbuf[0], STATE(corsaro)->number_mismatched_packets);
	bytes_htonl(&gbuf[4], attack_arr_cnt);
	bytes_htonl(&gbuf[8], 
		    kh_size(STATE(corsaro)->attack_hash)-attack_arr_cnt);
	if(corsaro_file_write(corsaro, corsaro->global_file,
			    &gbuf[0], 12) != 12)
	  {
	    corsaro_log(__func__, corsaro, 
			"could not dump global stats to file");
	    return -1;
	  }
	
	/* dump the number of vectors */
	bytes_htonl(&cntbuf[0], attack_arr_cnt);
	if(corsaro_file_write(corsaro, STATE(corsaro)->outfile, 
			    &cntbuf[0], 4) != 4)
	  {
	    corsaro_log(__func__, corsaro, 
			"could not dump vector count to file");
	    return -1;
	  }
	/* dump the vectors */
	for(i = 0; i < attack_arr_cnt; i++)
	  {
	    if(binary_dump(corsaro, attack_arr[i]) != 0)
	      {
		corsaro_log(__func__, corsaro, "could not dump hash");
		return -1;
	      }
	    attack_vector_reset(attack_arr[i]);
	  }
      }
  else
    {
      corsaro_log(__func__, corsaro, "invalid mode");
      return -1;
    }
  corsaro_io_write_plugin_end(corsaro, corsaro->global_file, PLUGIN(corsaro));
  corsaro_io_write_interval_end(corsaro, STATE(corsaro)->outfile, int_end);

  STATE(corsaro)->number_mismatched_packets = 0;
  
  free(attack_arr);

  /* if we are rotating, now is when we should do it */
  if(corsaro_is_rotate_interval(corsaro))
    {
      /* close the current file */
      if(STATE(corsaro)->outfile != NULL)
	{
	  corsaro_file_close(corsaro, STATE(corsaro)->outfile);
	  STATE(corsaro)->outfile = NULL;
	}
    }

  return 0;
}

int corsaro_dos_process_packet(corsaro_t *corsaro, 
			     corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  void *temp = NULL;
  uint8_t proto;
  uint32_t remaining;

  libtrace_ip_t *ip_hdr = NULL;
  libtrace_icmp_t *icmp_hdr = NULL;
  libtrace_ip_t *inner_ip_hdr = NULL;

  /* borrowed from libtrace's protocols.h (used by trace_get_*_port) */
  struct ports_t {
    uint16_t src;           /**< Source port */
    uint16_t dst;           /**< Destination port */
  };

  uint16_t attacker_port = 0;
  uint16_t target_port = 0;
  
  attack_vector_t findme;

  int khret;
  khiter_t khiter;
  attack_vector_t *vector = NULL;
  uint8_t *pkt_buf = NULL;
  libtrace_linktype_t linktype;

  struct timeval tv;

  if((packet->state.flags & CORSARO_PACKET_STATE_FLAG_BACKSCATTER) == 0)
    {
      /* not a backscatter packet */
      return 0;
    }

  /* backscatter packet, lets find the flow */
  /* check for ipv4 */
  /* 10/19/12 ak replaced much more verbose code to get header with this */
  if((ip_hdr = trace_get_ip(ltpacket)) == NULL)
    {
      /* non-ipv4 packet */
      return 0;
    }

  /* get the transport header */
  if((temp = trace_get_transport(ltpacket, &proto, &remaining)) == NULL)
    {
      /* not enough payload */
      return 0;
    }

  findme.target_ip = 0;

  if(ip_hdr->ip_p == TRACE_IPPROTO_ICMP && remaining >= 2)
    {
      icmp_hdr = (libtrace_icmp_t *)temp;

      if((icmp_hdr->type == 3  ||
	  icmp_hdr->type == 4  ||
	  icmp_hdr->type == 5  ||
	  icmp_hdr->type == 11 ||
	  icmp_hdr->type == 12) && 
	 ((temp = trace_get_payload_from_icmp(icmp_hdr, &remaining)) != NULL
	 && remaining >= 20 && (inner_ip_hdr = (libtrace_ip_t *)temp) &&
	  inner_ip_hdr->ip_v == 4))
	{
	  /* icmp error message */
	  if(inner_ip_hdr->ip_src.s_addr != ip_hdr->ip_dst.s_addr)
	    {
	      STATE(corsaro)->number_mismatched_packets++;
	    }

	  findme.target_ip = ntohl(inner_ip_hdr->ip_dst.s_addr);

	  /* just extract the first four bytes of payload as ports */
	  if((temp = trace_get_payload_from_ip(inner_ip_hdr, NULL,
					       &remaining)) != NULL
	     && remaining >= 4)
	    {
	      attacker_port = ntohs(((struct ports_t *)temp)->src);
	      target_port = ntohs(((struct ports_t *)temp)->dst);
	    }
	}
      else
	{
	  findme.target_ip =  ntohl(ip_hdr->ip_src.s_addr);
	  attacker_port = ntohs(icmp_hdr->code);
	  target_port = ntohs(icmp_hdr->type);
	}
    }
  else if((ip_hdr->ip_p == TRACE_IPPROTO_TCP || 
	  ip_hdr->ip_p == TRACE_IPPROTO_UDP) && 
	  remaining >= 4)
    {
      findme.target_ip = ntohl(ip_hdr->ip_src.s_addr);
      attacker_port = trace_get_destination_port(ltpacket);
      target_port = trace_get_source_port(ltpacket);
    }

  if(findme.target_ip == 0)
    {
      /* the packet is none of ICMP, TCP or UDP */
      return 0;
    }

  tv = trace_get_timeval(ltpacket);

  /* is this vector in the hash? */
  assert(STATE(corsaro)->attack_hash != NULL);
  if((khiter = kh_get(av, STATE(corsaro)->attack_hash, &findme)) 
     != kh_end(STATE(corsaro)->attack_hash))
    {
      /* the vector is in the hash */
      vector = kh_key(STATE(corsaro)->attack_hash, khiter);

      if(attack_vector_is_expired(vector, tv.tv_sec) != 0)
	{
	  kh_del(av, STATE(corsaro)->attack_hash, khiter);
	  attack_vector_free(vector);
	  vector = NULL;
	}
    }

  if(vector == NULL)
    {
      /* create a new vector and fill it */
      if((vector = attack_vector_init(corsaro)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "failed to create new attack vector");
	  return -1;
	}

      /* i think this may be buggy. do it the safe way for now
      vector->initial_packet = corsaro_mincopy_packet(packet);
      */
      vector->initial_packet_len = trace_get_capture_length(ltpacket);

      if((vector->initial_packet = malloc(vector->initial_packet_len)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not malloc initial packet");
	  return -1;
	}

      if((pkt_buf = trace_get_packet_buffer(ltpacket,
					    &linktype, NULL)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not get packet buffer");
	  return -1;
	}

      memcpy(vector->initial_packet, pkt_buf, vector->initial_packet_len);

      vector->attacker_ip = ntohl(ip_hdr->ip_dst.s_addr);
      vector->responder_ip = ntohl(ip_hdr->ip_src.s_addr);
      vector->target_ip = findme.target_ip;
      
      vector->start_time = tv;

      vector->ppm_window.window_start = tv.tv_sec;

      /* add to the hash */
      khiter = kh_put(av, STATE(corsaro)->attack_hash, vector, &khret);
    }

  assert(vector != NULL);

  vector->packet_cnt++;
  vector->interval_packet_cnt++;
  vector->byte_cnt += ntohs(ip_hdr->ip_len);
  vector->interval_byte_cnt += ntohs(ip_hdr->ip_len);

  vector->latest_time = tv;
  /* update the pps window */
  attack_vector_update_ppm_window(vector, tv);
  
  /* add the attacker ip to the hash */
  kh_put(32xx, vector->attack_ip_hash, ntohl(ip_hdr->ip_dst.s_addr), &khret);
  
  /* add the ports to the hashes */
  kh_put(32xx, vector->attack_port_hash, attacker_port, &khret);
  kh_put(32xx, vector->target_port_hash, target_port, &khret);

  return 0;
}

/* ==== External Output Convenience Functions ==== */

void corsaro_dos_attack_vector_get_packet(
			    corsaro_dos_attack_vector_in_t *attack_vector, 
			    libtrace_packet_t *packet)
{
  assert(packet != NULL);

  trace_construct_packet(packet, TRACE_TYPE_ETH, 
			 attack_vector->initial_packet, 
			 attack_vector->initial_packet_len);
}

off_t corsaro_dos_global_header_fprint(corsaro_t *corsaro, 
				corsaro_file_t *file, 
				corsaro_dos_global_header_t *header)
{
  assert(corsaro != NULL);
  assert(file != NULL);
  assert(header != NULL);

  return corsaro_file_printf(corsaro, file, 
			     "mismatch: %"PRIu32"\n"
			     "attack_vectors: %"PRIu32"\n"
			     "non-attack_vectors: %"PRIu32"\n", 
			     header->mismatched_pkt_cnt,
			     header->attack_vector_cnt,
			     header->non_attack_vector_cnt);

}

void corsaro_dos_global_header_print(corsaro_dos_global_header_t *header)
{
  assert(header != NULL);
  
  fprintf(stdout, "mismatch: %"PRIu32"\n"
	  "attack_vectors: %"PRIu32"\n"
	  "non-attack_vectors: %"PRIu32"\n", 
	  header->mismatched_pkt_cnt,
	  header->attack_vector_cnt,
	  header->non_attack_vector_cnt);
}

/**
 * @todo extend libpacketdump to allow to dump to a file
 */
off_t corsaro_dos_attack_vector_fprint(corsaro_t *corsaro, 
				       corsaro_file_t *file,
				       corsaro_dos_attack_vector_in_t *av)
{
  uint32_t tmp;
  char t_ip[16];

  assert(corsaro != NULL);
  assert(file != NULL);
  assert(av != NULL);

  tmp = htonl(av->target_ip);
  inet_ntop(AF_INET,&tmp, &t_ip[0], 16);

  return corsaro_file_printf(corsaro, file, 
			     "%s"
			     ",%"PRIu32
			     ",%"PRIu32
			     ",%"PRIu32
			     ",%"PRIu32
			     ",%"PRIu64
			     ",%"PRIu32
			     ",%"PRIu64
			     ",%"PRIu32
			     ",%"PRIu64
			     ",%"PRIu32".%06"PRIu32
			     ",%"PRIu32".%06"PRIu32
			     "\n",
			     t_ip,
			     av->attacker_ip_cnt,
			     av->interval_attacker_ip_cnt, 
			     av->attack_port_cnt, 
			     av->target_port_cnt,
			     av->packet_cnt,
			     av->interval_packet_cnt,
			     av->byte_cnt,
			     av->interval_byte_cnt,
			     av->max_ppm,
			     av->start_time_sec, 
			     av->start_time_usec,
			     av->latest_time_sec, 
			     av->latest_time_usec);
  
}

void corsaro_dos_attack_vector_print(corsaro_dos_attack_vector_in_t *av)
{
  uint32_t tmp;
  char t_ip[16];
  libtrace_packet_t *packet;
 
  assert(av != NULL);

  tmp = htonl(av->target_ip);
  inet_ntop(AF_INET,&tmp, &t_ip[0], 16);

  fprintf(stdout,
	  "%s"
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32
	  ",%"PRIu64
	  ",%"PRIu32".%06"PRIu32
	  ",%"PRIu32".%06"PRIu32
	  "\n",
	  t_ip,
	  av->attacker_ip_cnt,
	  av->interval_attacker_ip_cnt, 
	  av->attack_port_cnt, 
	  av->target_port_cnt,
	  av->packet_cnt,
	  av->interval_packet_cnt,
	  av->byte_cnt,
	  av->interval_byte_cnt,
	  av->max_ppm,
	  av->start_time_sec, 
	  av->start_time_usec,
	  av->latest_time_sec, 
	  av->latest_time_usec);

  /* this may get slow if you are dumping *lots* of dos records */
  if ((packet = trace_create_packet()) == NULL) {
    corsaro_log_file(__func__, NULL, "error creating libtrace packet");
    return;
  }

  corsaro_dos_attack_vector_get_packet(av, packet);

#if HAVE_LIBPACKETDUMP
  fprintf(stdout, "START PACKET\n");
  trace_dump_packet(packet);
  fprintf(stdout, "\nEND PACKET\n");
#else
  fprintf(stdout, "corsaro not built with libpacketdump support\n"
	  "not dumping initial packet\n");
#endif
}

off_t corsaro_dos_header_fprint(corsaro_t *corsaro, 
				corsaro_file_t *file, 
				corsaro_dos_header_t *header)
{
  assert(corsaro != NULL);
  assert(file != NULL);
  assert(header != NULL);

  return corsaro_file_printf(corsaro, file, 
			     "%"PRIu32"\n", 
			     header->attack_vector_cnt);

}

void corsaro_dos_header_print(corsaro_dos_header_t *header)
{
  assert(header != NULL);
  
  fprintf(stdout, "%"PRIu32"\n", header->attack_vector_cnt);

}

off_t corsaro_dos_record_fprint(corsaro_t *corsaro, 
				corsaro_file_t *file, 
				corsaro_in_record_type_t record_type,
				corsaro_in_record_t *record)
{
  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER:
      return corsaro_dos_global_header_fprint(corsaro, file,
			    (corsaro_dos_global_header_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_HEADER:
      return corsaro_dos_header_fprint(corsaro, file, 
                            (corsaro_dos_header_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR:
      return corsaro_dos_attack_vector_fprint(corsaro, file,
			    (corsaro_dos_attack_vector_in_t *)record->buffer);
      break;

    default:
      corsaro_log(__func__, corsaro, "record_type %d not a dos record",
		record_type);
      return -1;
      break;
    }

  return -1;
}

int corsaro_dos_record_print(corsaro_in_record_type_t record_type,
			     corsaro_in_record_t *record)
{
  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_DOS_GLOBAL_HEADER:
      corsaro_dos_global_header_print(
                        (corsaro_dos_global_header_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_HEADER:
      corsaro_dos_header_print(
			(corsaro_dos_header_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_DOS_ATTACK_VECTOR:
      corsaro_dos_attack_vector_print(
		        (corsaro_dos_attack_vector_in_t *)record->buffer);
      break;

    default:
      corsaro_log_file(__func__, NULL, 
		       "record_type %d not a dos record",
		       record_type);
      return -1;
      break;
    }

  return 0;
}
