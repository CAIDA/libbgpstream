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


#ifndef __CORSARO_GEO_H
#define __CORSARO_GEO_H

#include "khash.h"

#include "corsaro_int.h"

KHASH_MAP_INIT_INT(corsaro_geo_rechash, struct corsaro_geo_record *)

/** @file
 *
 * @brief Header file dealing with the corsaro geolocation subsystem
 *
 * @author Alistair King
 *
 */

/** Structure which contains a geolocation record */
typedef struct corsaro_geo_record
{
  /** A unique ID for this record (used to join the Blocks and Locations Files)
   *
   * This should be considered unique only within a single geo provider type 
   * i.e. id's may not be unique across different corsaro_geo_provider_t objects
   */
  uint32_t id;

  /** 2 character string which holds the ISO2 country code */
  char country_code[3];

  /** Continent Code */
  int continent_code;
  
  /** 2 character string which represents the region the city is in */
  char region[3];

  /** String which contains the city name */
  char *city; 
  
  /** String which contains the postal code 
   * @note This cannot be an int as some countries (I'm looking at you, Canada)
   * use characters
   */
  char *post_code;

  /** Latitude of the city */
  double latitude;

  /** Longitude of the city */
  double longitude;

  /** Metro code */
  uint32_t metro_code;

  /** Area code */
  uint32_t area_code;

  /** Connection Speed/Type */
  char *conn_speed;

  /** Array of Autonomous System Numbers */
  uint32_t *asn;

  /** Number of ASNs in the asn array */
  int asn_cnt;

  /** Number of IP addresses that this ASN (or ASN group) 'owns' */
  uint32_t asn_ip_cnt;

  /* -- ADD NEW FIELDS ABOVE HERE -- */

  /** The next record in the list */
  struct corsaro_geo_record *next;

} corsaro_geo_record_t;

/** Should this provider be set to be the default geolocation result provider*/
typedef enum corsaro_geo_provider_default
  {
    /** This provider should *not* be the default geolocation result */
    CORSARO_GEO_PROVIDER_DEFAULT_NO   = 0,

    /** This provider should be the default geolocation result */
    CORSARO_GEO_PROVIDER_DEFAULT_YES  = 1,

  } corsaro_geo_provider_default_t;

/** A unique identifier for each geolocation provider that corsaro supports 
 *
 * @note Remember to add the provider name to provider_names in
 * corsaro_geo.c when you add a new provider ID above
 */
/** @todo move the provider names into an array matched by their ID */
typedef enum corsaro_geo_provider_id
  {
    /** Geolocation data from Maxmind (Geo or GeoLite) */
    CORSARO_GEO_PROVIDER_MAXMIND      =  1,

    /** Geolocation data from Net Acuity Edge */
    CORSARO_GEO_PROVIDER_NETACQ_EDGE  =  2,

    /** 'Geolocation' data from CAIDA pfx2as */
    CORSARO_GEO_PROVIDER_PFX2AS       = 3,

    /** Highest numbered geolocation provider ID */
    CORSARO_GEO_PROVIDER_MAX          = CORSARO_GEO_PROVIDER_PFX2AS,

  } corsaro_geo_provider_id_t;

typedef struct corsaro_geo_provider
{
  /** The ID of the provider */
  corsaro_geo_provider_id_t id;

  /** The name of the provider */
  const char *name;

  /** A hash of id => record for all allocated records of this provider */
  khash_t(corsaro_geo_rechash) *all_records;

  /** The datastructure that will be used to perform pfx => record lookups */
  struct corsaro_geo_datastructure *ds;

  /** The list of records which contain the results of geolocation using this
      provider */
  corsaro_geo_record_t *records;

} corsaro_geo_provider_t;

/** A unique identifier for each geolocation datastructure that corsaro
    supports */
typedef enum corsaro_geo_datastructure_id
  {
    /** Patricia Trie */
    CORSARO_GEO_DATASTRUCTURE_PATRICIA      =  1,

    /** @todo add Huge Array implementation */

    /** Highest numbered datastructure ID */
    CORSARO_GEO_DATASTRUCTURE_MAX          = CORSARO_GEO_DATASTRUCTURE_PATRICIA,

    /** Default Geolocation data-structure */
    CORSARO_GEO_DATASTRUCTURE_DEFAULT = CORSARO_GEO_DATASTRUCTURE_PATRICIA,

  } corsaro_geo_datastructure_id_t;

/** Structure which represents a geolocation datastructure */
typedef struct corsaro_geo_datastructure
{
  /** The ID of this datastructure */
  corsaro_geo_datastructure_id_t id;
  
  /** The name of this datastructure */
  char *name;

  /** Pointer to init function */
  int (*init)(corsaro_t *corsaro, struct corsaro_geo_datastructure *ds);
  
  void (*free)(struct corsaro_geo_datastructure *ds);

  int (*add_prefix)(corsaro_t *corsaro,
		    struct corsaro_geo_datastructure *ds, 
		    uint32_t addr, uint8_t mask,
		    corsaro_geo_record_t *record);

  corsaro_geo_record_t *(*lookup_record)(corsaro_t *corsaro,
					   struct corsaro_geo_datastructure *ds,
					   uint32_t addr);

  /** Pointer to a instance-specific state object */
  void *state;

} corsaro_geo_datastructure_t;

/** Get the provider name for the given ID
 *
 * @param id            The provider ID to retrieve the name for
 * @return the name of the provider, NULL if an invalid ID was provided
 */
const char *corsaro_geo_get_provider_name(corsaro_geo_provider_id_t id);

/** Get an array of provider names
 *
 * @return an array of provider names
 *
 * @note the number of elements in the array will be exactly
 * CORSARO_GEO_PROVIDER_MAX+1. The [0] element will be NULL.
 */
const char **corsaro_geo_get_provider_names();

/** Allocate a geolocation provider object in the packet state
 *
 * @param corsaro       The corsaro object to alloc the provider for
 * @param provider_id     The unique ID of the geolocation provider
 * @return the provider object created, NULL if an error occurred
 *
 * Plugins which implement a geolocation provider should call this function
 * inside their init_output function to allocate a provider object
 *
 * @note Default provider status overrides the requests of previous
 * plugins. Thus, the order in which users request the plugins to be run in can
 * have an effect on plugins which make use of the default provider
 * (e.g. corsaro_report).
 */
corsaro_geo_provider_t *corsaro_geo_init_provider(
				corsaro_t *corsaro,
				corsaro_geo_provider_id_t provider_id,
				corsaro_geo_datastructure_id_t ds_id,
				corsaro_geo_provider_default_t set_default);

/** Free the given geolocation provider object
 *
 * @param corsaro       The corsaro object to remove the provider from
 * @param provider        The geolocation provider object to free
 *
 * @note if this provider was the default, there will be *no* default provider set
 * after this function returns
 */
void corsaro_geo_free_provider(corsaro_t *corsaro,
			     corsaro_geo_provider_t *provider);

/** Allocate an empty geolocation record for the given id
 *
 * @param provider      The geolocation provider to associate the record with
 * @apram id            The id to use to inialize the record
 * @return the new geolocation record, NULL if an error occurred
 * 
 * @note Most geolocation providers will not want to allocate a record on the
 * fly for every packet, instead they will allocate all needed records at init
 * time, and then use corsaro_geo_provider_add_record to add the appropriate
 * record to the packet state structure. These records are stored in the
 * provider, and free'd when corsaro_geo_free_provider is called. Also *ALL*
 * char pointers in this structure will be free'd.
 */
corsaro_geo_record_t *corsaro_geo_init_record(corsaro_geo_provider_t *provider,
					      uint32_t id);

/** Get the geolocation record for the given id
 *
 * @param provider      The geolocation provider to retrieve the record from
 * @apram id            The id of the record to retrieve
 * @return the corresponding geolocation record, NULL if an error occurred
 */
corsaro_geo_record_t *corsaro_geo_get_record(corsaro_geo_provider_t *provider,
					     uint32_t id);

/** Get an array of all the geolocation records registered with the given
 *  provider
 *
 * @param provider      The geolocation provider to retrieve the records from
 * @param(out) records  Returns an array of geolocation objects
 * @return the number of records in the array, -1 if an error occurs
 *
 * @note This function allocates and populates the array dynamically, so do not
 * call repeatedly. Also, it is the caller's responsibility to free the array.
 * DO NOT free the records contained in the array.
 */
int corsaro_geo_get_all_records(corsaro_geo_provider_t *provider,
				corsaro_geo_record_t ***records);


/** Register a new prefix to record mapping for the given provider
 *
 * @param corsaro       The corsaro object associated with the provider
 * @param provider      The provider to register the mapping with
 * @param addr          The network byte-ordered component of the prefix
 * @param mask          The mask component of the prefix
 * @param record        The record to associate with the prefix
 * @return 0 if the prefix is successfully associated with the prefix, -1 if an
 * error occurs
 */
int corsaro_geo_provider_associate_record(corsaro_t *corsaro, 
					  corsaro_geo_provider_t *provider,
					  uint32_t addr,
					  uint8_t mask,
					  corsaro_geo_record_t *record);

/** Look up the given address in the provider's datastructure
 *
 * @param corsaro       The corsaro object associated with the provider
 * @param provider      The provider to perform the lookup with
 * @param addr          The address to retrieve the record for 
                        (network byte ordering)
 * @return the record which best matches the address, NULL if no record is found
 */
corsaro_geo_record_t *corsaro_geo_provider_lookup_record(corsaro_t *corsaro, 
				       corsaro_geo_provider_t *provider,
				       uint32_t addr);

/** Remove all the existing records from the given geolocation provider
 *
 * @param provider        The geolocation provider to clear records for
 * @return the number of records cleared, -1 if an error occurs
 *
 * @note Typically this will be called by a geolocation provider for each
 * packet, before it calls corsaro_geo_provider_add_record to add the appropriate
 * record
 */
int corsaro_geo_provider_clear(corsaro_geo_provider_t *provider);

/** Add the given geolocation record to the head of the given geolocation provider
 * object
 *
 * @param provider        The geolocation provider object to add the record to
 * @param record        The geolocation record to add
 *
 * @note This function can be called multiple times to add multiple records to
 * the provider object. For example, there may be multiple ASes which a packet
 * could belong to.
 * @warning with great power comes great responsibility. If you add a record
 * more than once, it will cause a loop in the record list. Be careful.
 */
void corsaro_geo_provider_add_record(corsaro_geo_provider_t *provider,
				     corsaro_geo_record_t *record);


/** Retrieve the provider object for the default geolocation provider
 *
 * @param corsaro       The corsaro object to retrieve the provider object from
 * @return the provider object for the default provider, NULL if there is no
 * default provider
 */
corsaro_geo_provider_t *corsaro_geo_get_default(corsaro_t *corsaro);

/** Retrieve the provider object for the given provider ID
 *
 * @apram corsaro       The corsaro object to retrieve the provider object from
 * @param id            The geolocation provider ID to retrieve
 * @return the provider object for the given ID, NULL if there are no matches
 */
corsaro_geo_provider_t *corsaro_geo_get_by_id(corsaro_t *corsaro,
					    corsaro_geo_provider_id_t id);

/** Retrieve the provider object for the given provider name
 *
 * @apram corsaro       The corsaro object to retrieve the provider object from
 * @param id            The geolocation provider name to retrieve
 * @return the provider object for the given name, NULL if there are no matches
 */
corsaro_geo_provider_t *corsaro_geo_get_by_name(corsaro_t *corsaro,
					      const char *name);

/** Retrieve the next geolocation provider record in the list
 *
 * @param provider        The geolocation provider to get the next record for
 * @param record        The current record
 * @return the record which follows the current record, NULL if the end of the
 * record list has been reached. If record is NULL, the first record will be 
 * returned.
 */
corsaro_geo_record_t *corsaro_geo_next_record(corsaro_geo_provider_t *provider,
					      corsaro_geo_record_t *record);

/** Dump the given geolocation record to stdout (for debugging)
 *
 * @param record        The record to dump
 */
void corsaro_geo_dump_record(corsaro_geo_record_t *record);

/**
 * @name Provider-specific helper functions
 *
 * These are class functions that can be used to retrieve meta-data about
 * specific providers
 *
 * @{ */

/** Get the ISO-3166-1 2 character country code for the given maxmind country id
 *
 * @param country_id     The maxmind country id to get the code for
 * @return the 2 character country code
 */
const char *corsaro_geo_get_maxmind_iso2(int country_id);

/** Get a list of all possible ISO-3166-1 2 character country codes that maxmind
 *  uses
 *
 * @param(out) countries     Returns a pointer to an array of country codes
 * @return the number of countries in the array
 */
int corsaro_geo_get_maxmind_iso2_list(const char ***countries);

/** Get the ISO-3166-1 3 character country code for the given maxmind country id
 *
 * @param country_id     The maxmind country id to get the code for
 * @return  the 3 character country code
 */
const char *corsaro_geo_get_maxmind_iso3(int country_id);

/** Get a list of all possible ISO-3166-1 3 character country codes that maxmind
 *  uses
 *
 * @param(out) countries     Returns a pointer to an array of country codes
 * @return the number of countries in the array
 */
int corsaro_geo_get_maxmind_iso3_list(const char ***countries);

/** Get the country name for the given maxmind country id
 *
 * @param country_id     The maxmind country id to get the name for
 * @return the name of the country
 */
const char *corsaro_geo_get_maxmind_country_name(int country_id);

/** Get a list of all possible country names that maxmind uses
 *
 * @param(out) countries     Returns a pointer to an array of country codes
 * @return the number of countries in the array
 */
int corsaro_geo_get_maxmind_country_name_list(const char ***countries);

/** Get the continent code for the given maxmind country id
 *
 * @param country_id     The maxmind country id to get the continent for
 * @return the 2 character name of the continent
 */
const char *corsaro_geo_get_maxmind_continent(int country_id);

/** Get a mapping of continent codes that maxmind uses
 *
 * @param(out) countries     Returns a pointer to an array of continent codes
 * @return the number of countries in the array
 * 
 * @note The returned array should be used to map from the country array to
 * continents
 */
int corsaro_geo_get_maxmind_country_continent_list(const char ***continents);

/** @todo Add support for Net Acuity countries */

/** @} */

#endif /* __CORSARO_GEO_H */
