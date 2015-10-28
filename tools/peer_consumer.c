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

int main()
{

	Peer *msg;

	rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
	rd_kafka_conf_t *conf;
	rd_kafka_topic_conf_t *topic_conf;

	char *brokers = "192.172.226.44:9092";
	char *topic = "peers";
	int partition = 0,j=0;
	char errstr[512];
	int64_t start_offset = 0;
	rd_kafka_message_t *rkmessage;
	//uint8_t buf[MAX_MSG_SIZE];


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

		msg = peer__unpack(NULL,rkmessage->len,rkmessage->payload); // Deserialize the serialized input

		if(msg==NULL){
			printf("%s\n",rkmessage->payload);
		}
		else{
			printf("%d %s %d\n",msg->peerid_orig,msg->collector_str,msg->peer_asnumber);
		}
		rkmessage = rd_kafka_consume(rkt, partition, 1000);
		  peer__free_unpacked(msg, NULL);
		printf("\n");
	}
	  // Free the unpacked message

  return 0;
}
