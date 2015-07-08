#include "codes/codes_mapping.h"
#include "codes/lp-type-lookup.h"
#include "codes/jenkins-hash.h"
#include "codes/codes.h"
#include "codes/lp-msg.h"
#include <codes/model-net.h>
#include <assert.h>

/**** BEGIN SIMULATION DATA STRUCTURES ****/


/* 'magic' numbers used as sanity check on events */
static int node_magic; 
static int forwarder_magic;

/* counts of the various types of nodes in the example system */
static int num_burstBuffer_nodes, ;
static int num_burstBuffer_forwarders;

/* reqs to perform (provided by config file) */
static int num_reqs;
static uint64_t payload_sz;

/* network type for the various clusters */
static int net_id_client, net_id_svr, net_id_forwarding;

/* network type */
static int net_id;

/*The local disk bandwidth of pvfsFS on BlueGene/P*/
static float pvfs_tp_write_local_mu = 256.0 / 174;
static int pvfs_file_sz=0;
static char *param_group_nm = "server_pings";
static char *num_reqs_key = "num_reqs";
static char *payload_sz_key = "payload_sz";
static char *pvfs_file_sz_key = "pvfs_file_sz";

/* event types */
enum node_event
{
    NODE_KICKOFF = 123,
    NODE_RECV_req,
    NODE_RECV_ack,
};

typedef struct node_state_s {
    int is_in_client;    // whether we're in client's or svr's cluster
    int id_clust;     // my index within the cluster
    int num_processed;// number of requests processed
    tw_stime start_ts;    /* time that we started sending requests */
    tw_stime pvfs_ts_remote_write;		/*pvfsFS timestamp for local write*/
} node_state;


typedef struct node_msg_s {
    msg_header h;
    int id_clust_src;
} node_msg;

enum forwarder_event
{
    FORWARDER_FWD = 234,
    FORWARDER_RECV,
};

typedef struct forwarder_state_s {
    int id; // index w.r.t. forwarders in my group
    int is_in_client;
    int fwd_node_count;
    int fwd_forwarder_count;
} forwarder_state;

typedef struct forwarder_msg_s {
    msg_header h;
    int src_node_clust_id;
    int dest_node_clust_id;
    enum node_event node_event_type;
} forwarder_msg;



