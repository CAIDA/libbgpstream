#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"
#include "bgpstream.h"
#include "bgpstream_utils.h"
#include "bgprow.pb-c.h"
#include "peer.pb-c.h"
#include <librdkafka/rdkafka.h>
#include <errno.h>


#define MAX_MSG_SIZE 4096


/*static size_t
read_buffer (unsigned max_length, uint8_t *out)
{
  size_t cur_len = 0;
  size_t nread;
  while ((nread=fread(out + cur_len, 1, max_length - cur_len, stdin)) != 0)
  {
    cur_len += nread;
    if (cur_len == max_length)
    {
      fprintf(stderr, "max message length exceeded\n"); exit(1);
    }
  }
  return cur_len;
}*/

int main()
{

	BGPRow *msg;

	rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
	rd_kafka_conf_t *conf;
	rd_kafka_topic_conf_t *topic_conf;

	char *brokers = "192.172.226.44:9092";
	char *topic = "complete_view1";
	int partition = 0,j=0;
	char errstr[512];
	int64_t start_offset = 0;
	rd_kafka_message_t *rkmessage;
	  uint8_t buf[MAX_MSG_SIZE];


	conf = rd_kafka_conf_new();

	/* Topic configuration */
	topic_conf = rd_kafka_topic_conf_new();

	/* Create Kafka handle */
	if (!(rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf,errstr, sizeof(errstr)))) {
		fprintf(stderr,"%% Failed to create new consumer: %s\n",errstr);
		exit(1);
	}

	/* Add brokers */
	if (rd_kafka_brokers_add(rk, brokers) == 0) {
		fprintf(stderr, "%% No valid brokers specified\n");
		exit(1);
	}

	/* Create topic */
	rkt = rd_kafka_topic_new(rk, topic, topic_conf);

	/* Start consuming */
	if (rd_kafka_consume_start(rkt, partition, start_offset) == -1){
		fprintf(stderr, "%% Failed to start consuming: %s\n",rd_kafka_err2str(rd_kafka_errno2err(errno)));
		if (errno == EINVAL)
			fprintf(stderr,"%% Broker based offset storage requires a group.id, add: -X group.id=yourGroup\n");
		exit(1);
	}

	rkmessage = rd_kafka_consume(rkt, partition, 2000);
  printf("Receive:\n");
	while(rkmessage->len >0){
		j++;
		//printf("size of %d: %d\n",j++,(int)rkmessage->len);

		memcpy(buf,rkmessage->payload,rkmessage->len);
		msg = bgprow__unpack(NULL,rkmessage->len,buf); // Deserialize the serialized input

		if(msg==NULL){
			printf("%s\n",buf);
		}
		else{
		  bgpstream_pfx_t *pfx1 = (bgpstream_pfx_t *)msg->pfx.data;
		  bgpstream_ip_addr_t addr1 = pfx1->address;

		  char str[INET_ADDRSTRLEN+3];
		  if(addr1.version == BGPSTREAM_ADDR_VERSION_IPV4)
			  bgpstream_pfx_snprintf(str,INET_ADDRSTRLEN+3,pfx1);

		  printf("Pfx: %s\n",str);

		  printf("Peer_cnt: %zu\n",msg->n_cells);

		  int i;
		  for(i=0;i<msg->n_cells;i++){
			  bgpstream_peer_id_t peerid=*(msg->cells[i]->peerid.data);
			  printf("ID: %d\n",peerid);

			  printf("Len: %d\n",(int)msg->cells[i]->aspath.len);
		   	  bgpstream_as_path_t *mp = bgpstream_as_path_create();
			  bgpstream_as_path_populate_from_data(mp,msg->cells[i]->aspath.data, msg->cells[i]->aspath.len);

			 char str2[1024];
			 bgpstream_as_path_snprintf(str2, 1024, mp);
			 printf("Path: %s\n",str2);
		  }
		}
		rkmessage = rd_kafka_consume(rkt, partition, 1000);
		  bgprow__free_unpacked(msg, NULL);
		printf("\n");
	}
	  // Free the unpacked message

  return 0;
}
