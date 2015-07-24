/*
 * Copyright (C) 2014 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#include "codes-base/codes/codes_mapping.h"
#include "codes-base/codes/lp-type-lookup.h"
#include "codes-base/codes/jenkins-hash.h"
#include "codes-base/codes/codes.h"
#include "codes-base/codes/lp-msg.h"
#include "codes-net/codes/model-net.h"
#include <assert.h>

/**** BEGIN SIMULATION DATA STRUCTURES ****/

/* 'magic' numbers used as sanity check on events */
static int node_magic; 
static int forwarder_magic;

/* counts of the various types of nodes in the example system */
static int num_client_nodes, num_svr_nodes, num_burst_buffer_nodes;
static int num_client_forwarders, num_svr_forwarders, num_burst_buffer_forwarders;

/* reqs to perform (provided by config file) */
static int num_reqs;
static uint64_t payload_sz;
/* network type for the various clusters */
static int net_id_client, net_id_svr, net_id_forwarding, net_id_bb;

/* network type */
static int net_id;
/*The local disk bandwidth of pvfsFS on BlueGene/P*/
static float pvfs_tp_write_local_mu = 256.0 / 174;
static int pvfs_file_sz=0;
static char *param_group_nm = "server_pings";
static char *num_reqs_key = "num_reqs";
static char *payload_sz_key = "payload_sz";
static char *pvfs_file_sz_key = "pvfs_file_sz";

/*The local disk bandwidth of Burst Buffers*/
static float burst_buffer_local_mu;
static int bb_file_sz=0;
static char *size = "payload_sz";
static char *bb_file_size_key = "bb_file_sz";

/* event types */
enum node_event
{
    NODE_KICKOFF = 123,
    NODE_RECV_req,
    NODE_RECV_ack,
};

typedef struct node_state_s {
    int is_in_client;    // whether we're in client's cluster
    int is_in_server;	 // whether we're in svr's cluster
    int id_clust;        // my index within the cluster
    int num_processed;   // number of requests processed
    tw_stime start_ts;    /* time that we started sending requests */
    tw_stime pvfs_ts_remote_write;      /*pvfsFS timestamp for local write*/
    tw_stime bb_ts_remote_write; // bb timestamp for local write
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
    int is_in_server;
    int fwd_node_count;
    int fwd_forwarder_count;
} forwarder_state;

typedef struct forwarder_msg_s {
    msg_header h;
    int src_node_clust_id;
    int dest_node_clust_id;
    enum node_event node_event_type;
} forwarder_msg;

/**** END SIMULATION DATA STRUCTURES ****/

static tw_stime ns_to_s(tw_stime ns);
/**** BEGIN IMPLEMENTATIONS ****/

void node_lp_init(node_state * ns, tw_lp * lp){
    printf("In node_lp_init\n");
    ns->num_processed = 0;
    // nodes are addressed in their logical id space (0...num_client_nodes-1 and
    // 0...num_svr_nodes-1, respectively). LPs are computed upon use with
    // model-net, other events
    ns->id_clust = codes_mapping_get_lp_relative_id(lp->gid, 1, 0);
    int id_all = codes_mapping_get_lp_relative_id(lp->gid, 0, 0);

    // track which cluster we're in
    ns->is_in_client = (id_all < num_client_nodes);
    ns->is_in_server = (id_all < (num_svr_nodes + num_client_nodes) && (id_all >= num_client_nodes));

    printf("is_in_client=%d\nis_in_svr=%d\n",ns->is_in_client,ns->is_in_server);
    printf("id_all= %d\nnum_client_nodes= %d\n",id_all,num_client_nodes);
    // send a self kickoff event
    tw_event *e = codes_event_new(lp->gid, codes_local_latency(lp), lp);
    node_msg *m = tw_event_data(e);
    msg_set_header(node_magic, NODE_KICKOFF, lp->gid, &m->h);
    tw_event_send(e);
}

void node_finalize(node_state * ns,tw_lp * lp){
    // do some error checking - here, we ensure we got the expected number of
    // messages
    printf("In node_finalize\n");
    int mult;
    if (ns->is_in_client){
        mult = 1;
    }
    else /*if(ns->is_in_server)*/{
        //printf("num_svr_nodes is %d\n",num_svr_nodes);
        mult = (num_client_nodes / num_svr_nodes) + ((num_client_nodes % num_svr_nodes) > ns->id_clust);
    }
    if (ns->num_processed != num_reqs*mult){
        fprintf(stderr,"%s node %d, lp %lu: processed %d (expected %d)\n",ns->is_in_client ? "client" : "svr", ns->id_clust, lp->gid,ns->num_processed, num_reqs*mult);
    }

    float io_noise = 0.05 * tw_rand_integer(lp->rng,ns->pvfs_ts_remote_write,ns->pvfs_ts_remote_write);
    float io_moise_bb = 0.05* tw_rand_integer(lp->rng,ns->bb_ts_remote_write,ns->bb_ts_remote_write);

    long rand_idx = 0;
    //printf("num_svr_nodes is %d\n",num_svr_nodes);
    int dest_id = (lp->gid + rand_idx * 2) % (num_svr_nodes * 2);
    printf("Server %llu time = %f seconds.\n", (unsigned long long)(dest_id/2), ns_to_s(tw_now(lp)-ns->start_ts)+io_noise);

            return;
}

/* event type handlers */
void handle_node_next(node_state * ns,node_msg * m,tw_lp * lp){
    printf("In handle_node_next\n");
    // we must be in cluster client for this function
    assert(ns->is_in_client);

    // generate a message to send to the forwarder
    forwarder_msg m_fwd;
    msg_set_header(forwarder_magic, FORWARDER_FWD, lp->gid, &m_fwd.h);

    m_fwd.src_node_clust_id  = ns->id_clust;
    // compute the destination in cluster svr to req based on a simple modulo
    // of the logical indexes
    //printf("num_svr_nodes is %d\n",num_svr_nodes);
    m_fwd.dest_node_clust_id = ns->id_clust % num_svr_nodes;
    //printf("dest_node_clust_id is %d\n",m_fwd.dest_node_clust_id);
    m_fwd.node_event_type = NODE_RECV_req;

    // compute the dest forwarder index, again using a simple modulo
    //printf("num_client_forwarders is %d\n",num_client_forwarders);
    int dest_fwd_id = ns->id_clust % num_client_forwarders;
   // printf("dest_node_fwd_id is %d\n",dest_fwd_id);

    // as the relative forwarder IDs are with respect to groups, the group
    // name must be used
    tw_lpid dest_fwd_lpid = codes_mapping_get_lpid_from_relative(dest_fwd_id,"client_FORWARDERS", "forwarder", NULL, 0);
    //printf("dest_fwd_lpid is %d\n",(int)dest_fwd_lpid);
    // as cluster nodes have only one network type (+ annotation), no need to
    // use annotation-specific messaging
    model_net_event_annotated(net_id_client, "client","req", dest_fwd_lpid, payload_sz, 0.0,sizeof(m_fwd), &m_fwd, 0, NULL, lp);
}

void handle_node_recv_req(node_state * ns,node_msg * m,tw_lp * lp){
    //printf("In handle_recv_req\n");
    // we must be in cluster svr to receive reqs
    assert(!ns->is_in_client);

    if(!ns->is_in_server && !ns->is_in_client){	//is in Burst_buffer
    printf("In handle_recv_req Busrt Buffer\n");
    // check that we received the msg from the expected source
    assert(m->id_clust_src % num_burst_buffer_nodes == ns->id_clust);

    // setup the response message through the forwarder
    forwarder_msg m_fwd;
    msg_set_header(forwarder_magic, FORWARDER_FWD, lp->gid, &m_fwd.h);

    m_fwd.src_node_clust_id = ns->id_clust;
    m_fwd.dest_node_clust_id = (m->id_clust_src % num_burst_buffer_nodes) % num_svr_nodes;
    m_fwd.node_event_type = NODE_RECV_ack;

    // compute the dest forwarder index, again using a simple modulus
    int dest_fwd_id = ns->id_clust % num_svr_forwarders;

    // as the relative forwarder IDs are with respect to groups, the group
    // name must be used
    tw_lpid dest_fwd_lpid = codes_mapping_get_lpid_from_relative(dest_fwd_id,"bb_FORWARDERS", "forwarder", NULL, 0);
    model_net_event_annotated(net_id_svr, "bb","ack", dest_fwd_lpid, pvfs_file_sz, 0.0,sizeof(m_fwd), &m_fwd, 0, NULL, lp);

    }
    else{											//is in server
    printf("In handle_recv_req Server\n");
    // check that we received the msg from the expected source
   // printf("In handle_node_recv_req num_svr_nodes is %d\n",num_svr_nodes);
    assert(m->id_clust_src % num_svr_nodes == ns->id_clust);

    // setup the response message through the forwarder
    forwarder_msg m_fwd;
    msg_set_header(forwarder_magic, FORWARDER_FWD, lp->gid, &m_fwd.h);

    m_fwd.src_node_clust_id = ns->id_clust;
    m_fwd.dest_node_clust_id = (m->id_clust_src % num_svr_nodes) % num_burst_buffer_nodes;
    //m_fwd.dest_node_clust_id = m->id_clust_src;
    m_fwd.node_event_type = NODE_RECV_req;			//TO CHANGE WITH BB
    //m_fwd.node_event_type = NODE_RECV_ack;

    // compute the dest forwarder index, again using a simple modulus
    //int dest_fwd_id = ns->id_clust % num_burst_buffer_forwarders;
    int dest_fwd_id = ns->id_clust % num_svr_forwarders;

    // as the relative forwarder IDs are with respect to groups, the group
    // name must be used
    tw_lpid dest_fwd_lpid = codes_mapping_get_lpid_from_relative(dest_fwd_id,"svr_FORWARDERS", "forwarder", NULL, 0);
    ns->pvfs_ts_remote_write += pvfs_tp_write_local_mu;
    model_net_event_annotated(net_id_svr, "svr","req", dest_fwd_lpid, pvfs_file_sz, 0.0,sizeof(m_fwd), &m_fwd, 0, NULL, lp);
    }

    ns->num_processed++;
}

void handle_node_recv_ack(node_state * ns,node_msg * m,tw_lp * lp){
    //printf("In handle_recv_ack\n");
    // we must be in cluster client
    //assert(ns->id_clust < num_client_nodes);
    if(ns->is_in_client){  // in client cluster
    	printf("In handle_recv_ack client\n");
    	// simply process the next message
    	ns->num_processed++;
    	if (ns->num_processed < num_reqs){
    		handle_node_next(ns, m, lp);
    	}
    }
    else if(ns->is_in_server){								  // in svr cluster
    	printf("In handle_recv_ack server\n");
    	// setup the response message through the forwarder
    	forwarder_msg m_fwd;
    	msg_set_header(forwarder_magic, FORWARDER_FWD, lp->gid, &m_fwd.h);
    	m_fwd.src_node_clust_id = ns->id_clust;
    	m_fwd.dest_node_clust_id = ns->id_clust % num_client_nodes;
    	m_fwd.node_event_type = NODE_RECV_ack;

    	// compute the dest forwarder index, again using a simple modulus
    	int dest_fwd_id = ns->id_clust % num_svr_forwarders;

    	// as the relative forwarder IDs are with respect to groups, the group
    	// name must be used
    	tw_lpid dest_fwd_lpid = codes_mapping_get_lpid_from_relative(dest_fwd_id,"svr_FORWARDERS", "forwarder", NULL, 0);
    	ns->pvfs_ts_remote_write += pvfs_tp_write_local_mu;
    	model_net_event_annotated(net_id_svr, "svr","ack", dest_fwd_lpid, pvfs_file_sz, 0.0,sizeof(m_fwd), &m_fwd, 0, NULL, lp);

    	ns->num_processed++;
    }
}

void handle_node_burst_buffer(node_state * ns,node_msg * m, tw_lp * lp){

	ns->num_processed++;
}

void node_event_handler(node_state * ns,tw_bf * b,node_msg * m,tw_lp * lp){
    printf("In node_event_handler\n");
    assert(m->h.magic == node_magic);

    switch (m->h.event_type){
        case NODE_KICKOFF:
            // nodes from client req to nodes in svr
            if (ns->is_in_client){
                handle_node_next(ns, m, lp);
            }
            break;
        case NODE_RECV_req:
            handle_node_recv_req(ns, m, lp);
            break;
        case NODE_RECV_ack:
            handle_node_recv_ack(ns, m, lp);
            break;
        /* ... */
        default:
            tw_error(TW_LOC, "node event type not known");
    }
}

/* ROSS function pointer table for this LP */
static tw_lptype node_lp = {
    (init_f) node_lp_init,
    (pre_run_f) NULL,
    (event_f) node_event_handler,
    (revent_f) NULL,
    (final_f)  node_finalize,
    (map_f) codes_mapping,
    sizeof(node_state),
};

void node_register(){
    //printf("In node_register\n");
    uint32_t h1=0, h2=0;

    bj_hashlittle2("node", strlen("node"), &h1, &h2);
    node_magic = h1+h2;

    lp_type_register("node", &node_lp);
}


/*** Forwarder LP ***/

void forwarder_lp_init(forwarder_state * ns,tw_lp * lp){
    printf("I node_forwarder_lp_init\n");
    // like nodes, forwarders in this example are addressed logically
    ns->id = codes_mapping_get_lp_relative_id(lp->gid, 1, 0);
    int id_all = codes_mapping_get_lp_relative_id(lp->gid, 0, 0);
    ns->is_in_client = (id_all < num_client_forwarders);
    ns->is_in_server = (id_all < (num_svr_forwarders + num_client_forwarders) && (id_all >= num_client_forwarders));
}

void forwarder_finalize(forwarder_state * ns,tw_lp * lp){
    printf("In forwarder_finalize\n");
    // nothing to see here
}

void handle_forwarder_fwd(forwarder_state * ns,forwarder_msg * m,tw_lp * lp){
    printf("In handle_forwarder_fwd\n");
    // compute the forwarder lpid to forward to
    int mod;
    const char * dest_group;
    char * category;
    if (ns->is_in_client){
        mod = num_svr_forwarders;
        dest_group = "svr_FORWARDERS";
        category = "req";
    }
    else if (ns->is_in_server){
    	if(m->node_event_type == NODE_RECV_ack){
    		//printf("Test forwarder ACK\n");
    		mod = num_client_forwarders;
    		dest_group = "client_FORWARDERS";
    		category = "ack";
    	}
    	else if(m->node_event_type == NODE_RECV_req){
    		//printf("Test forwarder REQ\n");
    		mod = num_burst_buffer_forwarders;
    		dest_group = "bb_FORWARDERS";
    		category = "req";
    	}
    	}
    else{
    	mod = num_svr_forwarders;
    	dest_group = "svr_FORWARDERS";
    	category = "ack";
    }

    // compute the ROSS id corresponding to the dest forwarder
    //printf("mod is %d\n",mod);
    tw_lpid dest_lpid = codes_mapping_get_lpid_from_relative(ns->id % mod, dest_group, "forwarder", NULL, 0);

    forwarder_msg m_fwd = *m;
    msg_set_header(forwarder_magic, FORWARDER_RECV, lp->gid, &m_fwd.h);

    // here, we need to use the unannotated forwarding network, so we
    // use the annotation version of model_net_event
    model_net_event_annotated(net_id_forwarding, NULL, category, dest_lpid,payload_sz, 0.0, sizeof(m_fwd), &m_fwd, 0, NULL, lp);

    ns->fwd_node_count++;
}

void handle_forwarder_recv(forwarder_state * ns,forwarder_msg * m,tw_lp * lp) {
    printf("In handle_forwarder_recv\n");
    // compute the node to relay the message to
    const char * dest_group;
    const char * annotation;
    char * category;
    int net_id;

    if (ns->is_in_client){
        dest_group = "client_CLUSTER";
        annotation = "client";
        category = "ack";
        net_id=net_id_client;
    }
    else if(ns->is_in_server){
    	if(m->node_event_type == NODE_RECV_req){
    		dest_group = "svr_CLUSTER";
    		annotation = "svr";
    		category = "req";
    		net_id=net_id_svr;
    	}
    	else if(m->node_event_type == NODE_RECV_ack){
    		dest_group = "svr_CLUSTER";
    		annotation = "svr";
    		category = "ack";
    		net_id=net_id_svr;
    	}
    }
    else{
    	dest_group = "bb_CLUSTER";
    	annotation = "bb";
    	category = "req";
    	net_id=net_id_bb;
    }

    tw_lpid dest_lpid = codes_mapping_get_lpid_from_relative(m->dest_node_clust_id, dest_group, "node",NULL, 0);

    node_msg m_node;
    msg_set_header(node_magic, m->node_event_type, lp->gid, &m_node.h);
    m_node.id_clust_src = m->src_node_clust_id;

    // here, we need to use the client or svr cluster's internal network, so we use
    // the annotated version of model_net_event
    model_net_event_annotated(net_id, annotation, category, dest_lpid,payload_sz, 0.0, sizeof(m_node), &m_node, 0, NULL, lp);

    ns->fwd_forwarder_count++;
}

void forwarder_event_handler(forwarder_state * ns,tw_bf * b,forwarder_msg * m,tw_lp * lp){
    printf("In forwarder_event_handler\n");
    assert(m->h.magic == forwarder_magic);

    switch(m->h.event_type){
        case FORWARDER_FWD:
            handle_forwarder_fwd(ns, m, lp);
            break;
        case FORWARDER_RECV:
            handle_forwarder_recv(ns, m, lp);
            break;
        default:
            tw_error(TW_LOC, "unknown forwarder event type");
    }
}

static tw_lptype forwarder_lp = {
    (init_f) forwarder_lp_init,
    (pre_run_f) NULL,
    (event_f) forwarder_event_handler,
    (revent_f) NULL,
    (final_f)  forwarder_finalize,
    (map_f) codes_mapping,
    sizeof(forwarder_state),
};

void forwarder_register(){
    printf("In forwarder_register\n");
    uint32_t h1=0, h2=0;

    bj_hashlittle2("forwarder", strlen("forwarder"), &h1, &h2);
    forwarder_magic = h1+h2;

    lp_type_register("forwarder", &forwarder_lp);
}

/**** END IMPLEMENTATIONS ****/

/* arguments to be handled by ROSS - strings passed in are expected to be
 * pre-allocated */
static char conf_file_name[256] = {0};
/* this struct contains default parameters used by ROSS, as well as
 * user-specific arguments to be handled by the ROSS config sys. Pass it in
 * prior to calling tw_init */
const tw_optdef app_opt [] =
{
    TWOPT_GROUP("Model net test case" ),
        TWOPT_CHAR("codes-config", conf_file_name, "name of codes configuration file"),
    TWOPT_END()
};

static tw_stime s_to_ns(tw_stime ns)
{
    //printf("in s_to_ns\n");
    return(ns * (1000.0 * 1000.0 * 1000.0));
}

static tw_stime ns_to_s(tw_stime ns)
{
    //printf("In ns_to_s\n");
    return(ns / (1000.0 * 1000.0 * 1000.0));
}


int main(int argc, char *argv[])
{
    //printf("In main\n");
    g_tw_ts_end = s_to_ns(60*60*24*365); /* one year, in nsecs */

    /* ROSS initialization function calls */
    tw_opt_add(app_opt); /* add user-defined args */
    /* initialize ROSS and parse args. NOTE: tw_init calls MPI_Init */
    tw_init(&argc, &argv);

    if (!conf_file_name[0]) {
        tw_error(TW_LOC,"Expected \"codes-config\" option, please see --help.\n");
        return 1;
    }

    /* loading the config file into the codes-mapping utility, giving us the
     * parsed config object in return.
     * "config" is a global var defined by codes-mapping */
    if (configuration_load(conf_file_name, MPI_COMM_WORLD, &config)){
        tw_error(TW_LOC, "Error loading config file %s.\n", conf_file_name);
        return 1;
    }

    /* register model-net LPs with ROSS */
    model_net_register();

    /* register the user LPs */
    node_register();
    forwarder_register();

    /* setup the LPs and codes_mapping structures */
    codes_mapping_setup();

    /* setup the globals */
    int rc = configuration_get_value_int(&config, "server_pings", "num_reqs", NULL,&num_reqs);
    if (rc != 0)
        tw_error(TW_LOC, "unable to read server_pings:num_reqs");
    int payload_sz_d;
    	rc = configuration_get_value_int(&config, "server_pings", "payload_sz", NULL,&payload_sz_d);
    if (rc != 0)
        tw_error(TW_LOC, "unable to read server_pings:payload_sz");
    payload_sz = (uint64_t) payload_sz_d;

    /* get the counts for the client and svr clusters */
    num_client_nodes = codes_mapping_get_lp_count("client_CLUSTER", 0, "node",NULL, 1);
    num_svr_nodes = codes_mapping_get_lp_count("svr_CLUSTER", 0, "node",NULL, 1);
    num_burst_buffer_nodes = codes_mapping_get_lp_count("bb_CLUSTER", 0, "node",NULL, 1);
    num_client_forwarders = codes_mapping_get_lp_count("client_FORWARDERS", 0,"forwarder", NULL, 1);
    num_svr_forwarders = codes_mapping_get_lp_count("svr_FORWARDERS", 0,"forwarder", NULL, 1);
    num_burst_buffer_forwarders = codes_mapping_get_lp_count("bb_FORWARDERS", 0,"forwarder", NULL, 1);


    /* Setup the model-net parameters specified in the global config object,
         * returned are the identifier(s) for the network type.
         * 1 ID  -> all the same modelnet model
         * 2 IDs -> clusters are the first id, forwarding network the second
         * 3 IDs -> client is first, svr and bb second and forwarding network the third
         * 4 IDs -> cluster client is the first, svr is the second, burst buffer the third and forwarding network the last
         *          */
        int num_nets;
        int *net_ids = model_net_configure(&num_nets);
        assert(num_nets <= 4);
        if (num_nets == 1) {
            net_id_client = net_ids[0];
            net_id_svr = net_ids[0];
            net_id_bb = net_ids[0];
            net_id_forwarding = net_ids[0];
        }
        else if (num_nets == 2) {
            net_id_client = net_ids[0];
            net_id_svr = net_ids[0];
            net_id_bb = net_ids[0];
            net_id_forwarding = net_ids[1];
        }
        else if (num_nets == 3){
            net_id_client = net_ids[0];
            net_id_svr = net_ids[1];
            net_id_bb = net_ids[1];
            net_id_forwarding = net_ids[2];
        }
        else{
        	net_id_client = net_ids[0];
        	net_id_svr = net_ids[1];
        	net_id_bb = net_ids[2];
        	net_id_forwarding = net_ids[3];
        }
    free(net_ids);

        configuration_get_value_int(&config, param_group_nm, num_reqs_key, NULL,&num_reqs);
        configuration_get_value_int(&config, param_group_nm, payload_sz_key,NULL, (int *)&payload_sz);
        configuration_get_value_int(&config, param_group_nm, pvfs_file_sz_key, NULL,&pvfs_file_sz); /*Sughosh: added for pvfsfs*/
        configuration_get_value_int(&config, param_group_nm, bb_file_sz_key, NULL,&bb_file_sz); /*Tony: added for bb*/
    /* begin simulation */
        model_net_report_stats(net_id);
        tw_run();

    tw_end();

    return 0;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=8 sts=4 sw=4 expandtab
 */
