#include <stdio.h>
#include <inttypes.h>
#include <stdio.h>
#include <inttypes.h>

#include "utils.h"
#include "bgpstream_utils_addr.h"
#include "bgpstream_utils_ip_counter.h"

/* static char buffer[INET_ADDRSTRLEN]; */

/* IPv4 only */
typedef struct struct_pfx_int_t
{
  uint32_t start;
  uint32_t end;
  struct struct_pfx_int_t *next;
} pfx_int_t;


/* IP Counter Linked List */
struct bgpstream_ip_counter {
  pfx_int_t *head;
};


static void
one_step_merge(pfx_int_t *pil)
{
  pfx_int_t *current = pil;
  pfx_int_t *previous = pil;
  if(current->next == NULL)
    {
      return;
    }
  current = current->next;

  while(current != NULL)
    {
      if(current->start <= previous->end)
        {
          if(current->end > previous->end)
            {
              previous->end = current->end;
            }
          previous->next = current->next;
          free(current);
          current = previous->next;
        }
      else
        {
          break;
        }
    }
}

static void
merge_in_sorted_queue(pfx_int_t **pil, uint32_t start, uint32_t end)
{
  // create new pfx_int_t
  pfx_int_t *p = NULL;
  pfx_int_t *previous = *pil;
  pfx_int_t *current = *pil;

  while(current != NULL)
    {      
      if(start > current->end)
        {
          previous = current;
          current = current->next;
          continue;                   
        }
      if(end < current->start)
        {
          /* found a position where to insert  */
          p = (pfx_int_t *)malloc(sizeof(pfx_int_t));
          p->start = start;
          p->end = end;
          p->next = NULL;
          if(previous != current)
            {
              previous->next = p;              
            }
          else
            {
              *pil = p;              
            }
          p->next = current;
          return;
        }
      /* At this point S <= cE and E >= cS
       * so we can merge them */
      if(current->start > start)
        {
          current->start = start;
        }
      if(current->end < end)
        {
          current->end = end;
        }
      /* Now check from here on if there are other things
       * to merge */
      one_step_merge(current);
      return;
    }

  /*   if here we didn't find a place where to insert the 
   * interval, it is either the first position or the last */
  p = (pfx_int_t *)malloc(sizeof(pfx_int_t));
  p->start = start;
  p->end = end;
  p->next = NULL;  
  /* adding at the beginning of the queue */
  if (previous == current)
    {
      *pil = p;
      return;
    }
  // adding at the end of the queue
  previous->next = p;
}

/* static void */
/* print_pfx_int_list(pfx_int_t *pil) */
/* { */
/*   pfx_int_t *current = pil; */
/*   bgpstream_ipv4_addr_t ip; */
/*   ip.version = BGPSTREAM_ADDR_VERSION_IPV4; */

/*   while(current != NULL) */
/*     { */
/*       ip.ipv4.s_addr = htonl(current->start); */
/*       bgpstream_addr_ntop(buffer, INET_ADDRSTRLEN, &ip); */
/*       printf("IP space:\t%s\t", buffer); */
/*       ip.ipv4.s_addr = htonl(current->end); */
/*       bgpstream_addr_ntop(buffer, INET_ADDRSTRLEN, &ip); */
/*       printf("%s\n", buffer); */
/*       current = current->next; */
/*     } */
/* } */

bgpstream_ip_counter_t *
bgpstream_ip_counter_create()
{
  bgpstream_ip_counter_t *ipc;
  if((ipc = (bgpstream_ip_counter_t *)malloc_zero(sizeof(bgpstream_ip_counter_t))) == NULL)
    {
      return NULL;
    }
  ipc->head = NULL;
  return ipc;
}

void
bgpstream_ip_counter_add(bgpstream_ip_counter_t *ipc,
                         bgpstream_pfx_t *pfx)
{
  uint32_t start;
  uint32_t end;
  uint32_t len;

  if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      len = 32 - pfx->mask_len;
      start = ntohl(((bgpstream_ipv4_pfx_t *)pfx)->address.ipv4.s_addr);
      start = (start >> len) << len;
      end = 0;
      end = ~end;
      end = (end >> len) << len;
      end = start | (~end);
      merge_in_sorted_queue(&ipc->head, start, end);
    }

  /* print_pfx_int_list(ipc->head); */
}

uint32_t
bgpstream_ip_counter_is_overlapping(bgpstream_ip_counter_t *ipc,
                                    bgpstream_pfx_t *pfx)
{  
  pfx_int_t *current = ipc->head;
  uint32_t start = 0;
  uint32_t end = 0;
  uint32_t len = 0;
  uint32_t overlap_count = 0;
  if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      len = 32 - pfx->mask_len;
      start = ntohl(((bgpstream_ipv4_pfx_t *)pfx)->address.ipv4.s_addr);
      start = (start >> len) << len;
      end = 0;
      end = ~end;
      end = (end >> len) << len;
      end = start | (~end);
    }
  /* intersection endpoints */
  uint32_t int_start;
  uint32_t int_end;
  while(current != NULL)
    {
      if(current->start > end)
        {
          break;
        }
      if(current->end < start)
        {
          current = current->next;
          continue;
        }
      /* there is some overlap 
       * max(start) and min(end) */
      int_start = current->start;
      int_end = current->end;
      if(current->start < start)
        {
          int_start = start;
        }
      if(current->end > end)
        {
          int_end = end;
        }
      overlap_count += int_end - int_start + 1;
      current = current->next;      
    }
  return overlap_count;
}


uint32_t
bgpstream_ip_counter_get_ipcount(bgpstream_ip_counter_t *ipc)
{
  pfx_int_t *current = ipc->head;
  uint32_t ip_count = 0;  
  while(current != NULL)
    {
      ip_count += (current->end - current->start) + 1;
      current = current->next;
    }
  return ip_count;

}

void
bgpstream_ip_counter_clear(bgpstream_ip_counter_t *ipc)
{
  pfx_int_t *current = ipc->head;
  pfx_int_t *tmp = ipc->head;
  
  while(tmp != NULL)
    {
      current = tmp;
      tmp = tmp->next;
      free(current);
    }
  ipc->head = NULL;
}

void
bgpstream_ip_counter_destroy(bgpstream_ip_counter_t *ipc)
{
  bgpstream_ip_counter_clear(ipc);
  free(ipc);
}

