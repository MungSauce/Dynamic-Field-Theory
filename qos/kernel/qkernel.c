#include "qos/qkernel.h"

static void memzero(void *p, size_t n) {
    uint8_t *b=(uint8_t*)p;
    while(n--) *b++=0;
}

static int queue_empty(const qkernel_t *k){ return k->qhead==k->qtail; }
static int queue_push(qkernel_t *k, qevent_t e){
    uint16_t next=(uint16_t)((k->qtail+1u)%QOS_MAX_EVENTS);
    if(next==k->qhead) return -1;
    k->queue[k->qtail]=e; k->qtail=next; return 0;
}
static int queue_pop(qkernel_t *k, qevent_t *e){
    if(queue_empty(k)) return -1;
    *e=k->queue[k->qhead];
    k->qhead=(uint16_t)((k->qhead+1u)%QOS_MAX_EVENTS);
    return 0;
}

void qk_init(qkernel_t *k){ memzero(k,sizeof(*k)); }

int qk_is_allocated(const qkernel_t *k,uint16_t node){
    if(node>=QOS_MAX_NODES) return 0;
    return (k->allocated[node>>3]>>(node&7u))&1u;
}

static void alloc_bit(qkernel_t *k,uint16_t node,int value){
    uint8_t mask=(uint8_t)(1u<<(node&7u));
    if(value) k->allocated[node>>3]|=mask;
    else k->allocated[node>>3]&=(uint8_t)~mask;
}

qstate_t qk_get(const qkernel_t *k,uint16_t node){
    uint8_t byte=k->state[node>>2];
    return (qstate_t)((byte>>((node&3u)*2u))&3u);
}

int qk_set(qkernel_t *k,uint16_t node,qstate_t s){
    if(node>=QOS_MAX_NODES || !qk_is_allocated(k,node)) return -1;
    uint8_t shift=(uint8_t)((node&3u)*2u);
    uint8_t mask=(uint8_t)(3u<<shift);
    k->state[node>>2]=(uint8_t)((k->state[node>>2]&~mask)|((qstate_code(s)&3u)<<shift));
    return 0;
}

int qk_alloc(qkernel_t *k,uint16_t node,qstate_t initial){
    if(node>=QOS_MAX_NODES || qk_is_allocated(k,node)) return -1;
    alloc_bit(k,node,1);
    return qk_set(k,node,initial);
}

int qk_sever_node(qkernel_t *k,uint16_t node){
    if(node>=QOS_MAX_NODES || !qk_is_allocated(k,node)) return -1;
    alloc_bit(k,node,0);
    for(uint16_t i=0;i<k->edge_count;i++){
        if(k->edges[i].active && (k->edges[i].from==node || k->edges[i].to==node)) k->edges[i].active=0;
    }
    return 0;
}

int qk_link(qkernel_t *k,uint16_t from,uint16_t to,qedge_mode_t mode){
    if(!qk_is_allocated(k,from)||!qk_is_allocated(k,to)||k->edge_count>=QOS_MAX_EDGES) return -1;
    qedge_t *e=&k->edges[k->edge_count++];
    e->from=from;e->to=to;e->mode=(uint8_t)mode;e->active=1; return 0;
}

int qk_sever_link(qkernel_t *k,uint16_t from,uint16_t to){
    int found=0;
    for(uint16_t i=0;i<k->edge_count;i++){
        qedge_t *e=&k->edges[i];
        if(e->active&&e->from==from&&e->to==to){e->active=0;found=1;}
    }
    return found?0:-1;
}

int qk_strike(qkernel_t *k,uint16_t node,qstrike_t strike){
    if(!qk_is_allocated(k,node)) return -1;
    qevent_t e={node,(int8_t)strike}; return queue_push(k,e);
}

static qstrike_t invert(qstrike_t s){ return s==Q_STRIKE_POS?Q_STRIKE_NEG:Q_STRIKE_POS; }

static uint16_t queue_count(const qkernel_t *k){
    return k->qtail>=k->qhead ? (uint16_t)(k->qtail-k->qhead)
        : (uint16_t)(QOS_MAX_EVENTS-k->qhead+k->qtail);
}

static int mark_get(const qkernel_t *k,uint16_t node){
    return (k->pending_mark[node>>3]>>(node&7u))&1u;
}
static void mark_set(qkernel_t *k,uint16_t node,int value){
    uint8_t mask=(uint8_t)(1u<<(node&7u));
    if(value) k->pending_mark[node>>3]|=mask;
    else k->pending_mark[node>>3]&=(uint8_t)~mask;
}

static int propagate(qkernel_t *k,uint16_t node,qstrike_t strike){
    for(uint16_t i=0;i<k->edge_count;i++){
        qedge_t *e=&k->edges[i];
        if(!e->active || e->from!=node || !qk_is_allocated(k,e->to)) continue;
        qstrike_t out=strike;
        uint8_t repeats=1;
        if(e->mode==Q_EDGE_CANCEL) out=invert(out);
        else if(e->mode==Q_EDGE_DEEPEN) repeats=2;
        for(uint8_t r=0;r<repeats;r++){
            if(queue_push(k,(qevent_t){e->to,(int8_t)out})!=0) return -2;
        }
    }
    return 0;
}

int qk_run(qkernel_t *k,uint32_t event_budget){
    uint32_t done=0;
    while(!queue_empty(k)){
        uint16_t wave=queue_count(k);
        if(wave==0) break;
        if(done+(uint32_t)wave>event_budget) return 1; /* never split a causal wavefront */

        uint16_t touched_count=0;
        for(uint16_t i=0;i<wave;i++){
            qevent_t ev;
            if(queue_pop(k,&ev)!=0) return -3;
            done++; k->events_processed++;
            if(!qk_is_allocated(k,ev.node)) continue;
            if(!mark_get(k,ev.node)){
                mark_set(k,ev.node,1);
                k->touched[touched_count++]=ev.node;
            }
            k->pending[ev.node]+=(int16_t)ev.strike;
        }
        k->wavefronts_processed++;

        /* Only nodes touched by this wave are visited. Opposing strikes have
           already interfered in pending[]; zero net tension performs no work. */
        for(uint16_t i=0;i<touched_count;i++){
            uint16_t node=k->touched[i];
            int16_t net=k->pending[node];
            k->pending[node]=0; mark_set(k,node,0);
            if(net==0 || !qk_is_allocated(k,node)) continue;

            qstrike_t strike=net>0?Q_STRIKE_POS:Q_STRIKE_NEG;
            uint16_t strength=(uint16_t)(net>0?net:-net);
            for(uint16_t n=0;n<strength;n++){
                qstate_t before=qk_get(k,node);
                qstate_t after=q_apply(before,strike);
                if(after==before) break; /* saturation consumes remaining same-polarity tension */
                qk_set(k,node,after); k->transitions++;
                if(propagate(k,node,strike)!=0) return -2;
            }
        }
    }
    return 0;
}
