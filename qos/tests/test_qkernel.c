#include <stdio.h>
#include <stdlib.h>
#include "qos/qkernel.h"

#define CHECK(x) do{ if(!(x)){ fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } }while(0)

static int algebra(void){
    CHECK(q_apply(Q_NEG,Q_STRIKE_POS)==Q_PRIMED_ZERO);
    CHECK(q_apply(Q_PRIMED_ZERO,Q_STRIKE_POS)==Q_POS);
    CHECK(q_apply(Q_DECAY_ZERO,Q_STRIKE_POS)==Q_PRIMED_ZERO);
    CHECK(q_apply(Q_POS,Q_STRIKE_POS)==Q_POS);
    CHECK(q_apply(Q_POS,Q_STRIKE_NEG)==Q_DECAY_ZERO);
    CHECK(q_apply(Q_DECAY_ZERO,Q_STRIKE_NEG)==Q_NEG);
    CHECK(q_apply(Q_PRIMED_ZERO,Q_STRIKE_NEG)==Q_DECAY_ZERO);
    CHECK(q_apply(Q_NEG,Q_STRIKE_NEG)==Q_NEG);
    return 0;
}

int main(void){
    if(algebra()) return 1;
    qkernel_t k; qk_init(&k);
    CHECK(qk_alloc(&k,0,Q_PRIMED_ZERO)==0);
    CHECK(qk_alloc(&k,1,Q_DECAY_ZERO)==0);
    CHECK(qk_alloc(&k,2,Q_PRIMED_ZERO)==0);
    CHECK(qk_get(&k,0)==Q_PRIMED_ZERO);
    CHECK(qk_link(&k,0,1,Q_EDGE_CASCADE)==0);
    CHECK(qk_link(&k,1,2,Q_EDGE_CANCEL)==0);
    CHECK(qk_strike(&k,0,Q_STRIKE_POS)==0);
    CHECK(qk_run(&k,100)==0);
    CHECK(qk_get(&k,0)==Q_POS);
    CHECK(qk_get(&k,1)==Q_PRIMED_ZERO);
    CHECK(qk_get(&k,2)==Q_DECAY_ZERO);
    CHECK(k.transitions==3);
    CHECK(k.wavefronts_processed==3);
    CHECK(qk_sever_node(&k,1)==0);
    CHECK(!qk_is_allocated(&k,1));
    CHECK(qk_strike(&k,1,Q_STRIKE_POS)!=0);

    CHECK(qk_alloc(&k,3,Q_PRIMED_ZERO)==0);
    uint32_t before_cancel=k.transitions;
    CHECK(qk_strike(&k,3,Q_STRIKE_POS)==0);
    CHECK(qk_strike(&k,3,Q_STRIKE_NEG)==0);
    CHECK(qk_run(&k,100)==0);
    CHECK(qk_get(&k,3)==Q_PRIMED_ZERO);
    CHECK(k.transitions==before_cancel);

    CHECK(qk_alloc(&k,4,Q_NEG)==0);
    CHECK(qk_alloc(&k,5,Q_NEG)==0);
    CHECK(qk_link(&k,4,5,Q_EDGE_DEEPEN)==0);
    CHECK(qk_strike(&k,4,Q_STRIKE_POS)==0);
    CHECK(qk_run(&k,100)==0);
    CHECK(qk_get(&k,4)==Q_PRIMED_ZERO);
    CHECK(qk_get(&k,5)==Q_POS);
    puts("QOS_QKERNEL_CONFORMANCE=PASS");
    printf("transitions=%u events=%u\n",k.transitions,k.events_processed);
    return 0;
}
