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

int qk_run(qkernel_t *k,uint32_t event_budget){
    qevent_t ev;
    uint32_t done=0;
    while(done<event_budget && queue_pop(k,&ev)==0){
        done++; k->events_processed++;
        if(!qk_is_allocated(k,ev.node)) continue;
        qstate_t before=qk_get(k,ev.node);
        qstate_t after=q_apply(before,(qstrike_t)ev.strike);
        if(after==before) continue;
        qk_set(k,ev.node,after); k->transitions++;
        for(uint16_t i=0;i<k->edge_count;i++){
            qedge_t *e=&k->edges[i];
            if(!e->active || e->from!=ev.node || !qk_is_allocated(k,e->to)) continue;
            qstrike_t out=(qstrike_t)ev.strike;
            uint8_t repeats=1;
            if(e->mode==Q_EDGE_CANCEL) out=invert(out);
            else if(e->mode==Q_EDGE_DEEPEN) repeats=2;
            for(uint8_t r=0;r<repeats;r++){
                if(queue_push(k,(qevent_t){e->to,(int8_t)out})!=0) return -2;
            }
        }
    }
    return queue_empty(k)?0:1;
}
