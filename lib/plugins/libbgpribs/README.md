
============================ COLLECTOR_DATA ============================

status:
	COLLECTOR_NULL:
	we do not know what is the status of the collector
	we are (most likely) at the beginning of the stream 
	and we did not have enough information to build a
	consistent status.

	possible changes:
	COLLECTOR_NULL -> COLLECTOR_UP when we finally see at
	least 1 peer having a consistent status


	COLLECTOR_UP:
	there is at least 1 peer that has a consistent 
	status. so the collector is up

	possible changes:
	COLLECTOR_UP -> COLLECTOR_DOWN when there are no peers
	having a consistent status 


	COLLECTOR_DOWN:
	no peer has a consistent status, so the collector is down

	possible changes:
	COLLECTOR_DOWN -> COLLECTOR_UP when there is at least one
	peer having a consistent status 




============ COLLECTOR_DATA -> PEER_DATA PROTOCOL ==================


every time a collector_data object receives a record, the following
actions are taken:

	1) if the record is VALID we extract the elem_queue 
	   then, for each elem we create a peer (if necessary) and 
	   we call the following function:

		int apply_elem(peer_data, bs_record, bs_elem)	

	   the function returns:
	   -1 if something went wrong during the execution
	    0 otherwise

	2) for each peer in the peers_table (which now includes peers
	   that could have been created at the previous step) we
	   call the function:

		   int apply_record(peer_data, bs_record) 

	   the function returns:
	   -1 if something went wrong during the execution
	    0 if the peer is currently down
	    1 if the peer is currently up


Notes/comments:
	why do we use the bs_record in the apply_elem function ?
	- because we need to know when we are processing a dump_start
	  or a dump_end before applying the elem
	
	why do we send the bs_record twice? 
	- record information are useful to update the time information
	  of each peer even if such a peer has not been affected by any
	  elem in the last run
	- the reason why we need to process all the elem before applying
	  the record is due to the fact that elem may create new peers
	  thus, all of them need to receive this information



============================ PEER_DATA ============================

Every peer_data takes care of two ribs_tables:


ACTIVE RIBS TABLE (a_rt):
	is the current consistent ribs table	


UNDER CONSTRUCTION RIBS TABLE (uc_rt):
	is the one which is currently under construction


	When a new rib is received the uc_rt is populated, 
	however any update message during this interval is
	applied to both tables. Also at any point in time
	if a dump is requested, stats will be always taken
	from the active tables.

	When a rib_dump_end message is received, the construction
	of the uc_rt is terminated and uc_rt becomes the new a_rt,
	while the uc_rt is cleared/reset (we want to clear the
	structure but we do not want to dealloacate memory).


status:
	PEER_NULL:
	we are not able to provide any information about the peer
	ribs at this moment. Every time we enter in this state we
	reset the active ribs.
	Common cases are:
	- we are at the beginning of the processing, we haven't
	   built a consistent rib in memory yet
	- we received a corrupted record that invalidated the current
	  status and we haven't rebuilt a consistent rib table yet.
    - we received an out of order message and we were not
	  able to rollback properly

	possible changes:
	PEER_NULL -> PEER_UP:
	- the rib in memory is finally consistent
	PEER_NULL -> PEER_DOWN:
	- 	we receive a peer down message
		

	PEER_UP:
	there is a consistent peer ribs_table in memory (the active one)
	at this moment.

	possible changes:
	PEER_UP -> PEER_NULL:
	- we receive corrupted record message that affects the current
	   status
	PEER_UP -> PEER_DOWN:
	- we receive a peer_down state message
	- we do not receive updates for "XXX" minutes


	PEER_DOWN:
	The peer is not active at this moment, the ribs table are
	empty and they are not exported

	possible changes:
	PEER_DOWN -> PEER_UP:
	- we receive a peer_up state message
	- we receive a new update
	PEER_DOWN -> PEER_NULL:
	- we start receiving a "valid rib", the status is not down
	  anymore neither is consistent though


ribs_tables status

status:
	UC_ON: under construction
	- when the peer is up we apply updates to both the
	   uc and active table
	- when the peer is null we apply updates to the uc
	   table only
    - when the peer is down and UC is activated then we
	   move to the NULL state
    UC_OFF:
	- when the peer is up we apply updates to the
	   active table only


============================ RIBS_TABLE ============================

Example of a current status:

	       RIB        UPDATES
	{xxx[**-*-**]xxx}--------------------

Legend:
        {  RIB record, DUMP_START
	   reference_dump_time is taken from this record

	}  RIB record, DUMP_END
           it determines when to swap uc_rt and a_rt

	x  elems extracted after the DUMP_START or before
	   the DUMP_END that are not applicable to the 
	   current peer

	[  first element after the DUMP_START to be
	   applied to the current peer
	   variable: reference_rib_start

	]  last element before the DUMP_END that has been
	   applied to the current peer
	   variable: reference_rib_end

	*  valid && in-time rib elems

	-  valid && in-time update elems


	


============================ INTEGRATED ROLLBACK ============================

This is a soft rollback implementation, it means that:
a) plugin is not going back in the past to rewrite history!
b) however it is applying data to the rib so that the rib contains
   the most accurate data at every instant in time


how it works:

    --> rib
    - every prefix in the rib table has a ts associated with the last
      elem that provided information about it
    - also, there is a flag that tells if the prefix is active or not

    --> when we receive an update at time ts > active_rib_start
        then we consider the following options:

- if it is an announcement:   
1) we check if the rib table contains the prefix already
2) we get the prefix time as stated in the rib (p_rib_time)
3) we get the prefix status as stated in the rib (active | inactive)
4) if(p_rib_time <= ts): => apply update and turn p active       
   else: do nothing (p is already up to date)

- if it is a withdrawal:   
1) we check if the rib table contains the prefix already
2) we get the prefix time as stated in the rib (p_rib_time)
3) we get the prefix status as stated in the rib (active | inactive)
4) if(p_rib_time <= ts): => apply update and turn p inactive       
   else: do nothing (p is already up to date)





