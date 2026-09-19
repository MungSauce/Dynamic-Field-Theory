#ifndef QOS_QKERNEL_H
#define QOS_QKERNEL_H
#include <stdint.h>
#include <stddef.h>
#include "qstate.h"

#ifndef QOS_MAX_NODES
#define QOS_MAX_NODES 1024u
#endif
#ifndef QOS_MAX_EDGES
#define QOS_MAX_EDGES 4096u
#endif
#ifndef QOS_MAX_EVENTS
#define QOS_MAX_EVENTS 8192u
#endif

typedef enum {
    Q_EDGE_CASCADE = 0,
    Q_EDGE_DEEPEN  = 1,
    Q_EDGE_CANCEL  = 2
} qedge_mode_t;

typedef struct {
    uint16_t from;
    uint16_t to;
    uint8_t mode;
    uint8_t active;
} qedge_t;

typedef struct {
    uint16_t node;
    int8_t strike;
} qevent_t;

typedef struct {
    uint8_t state[(QOS_MAX_NODES + 3u) / 4u];
    uint8_t allocated[(QOS_MAX_NODES + 7u) / 8u];
    qedge_t edges[QOS_MAX_EDGES];
    uint16_t edge_count;
    qevent_t queue[QOS_MAX_EVENTS];
    uint16_t qhead;
    uint16_t qtail;
    uint32_t transitions;
    uint32_t events_processed;
} qkernel_t;

void qk_init(qkernel_t *k);
int qk_alloc(qkernel_t *k, uint16_t node, qstate_t initial);
int qk_sever_node(qkernel_t *k, uint16_t node);
int qk_is_allocated(const qkernel_t *k, uint16_t node);
qstate_t qk_get(const qkernel_t *k, uint16_t node);
int qk_set(qkernel_t *k, uint16_t node, qstate_t s);
int qk_link(qkernel_t *k, uint16_t from, uint16_t to, qedge_mode_t mode);
int qk_sever_link(qkernel_t *k, uint16_t from, uint16_t to);
int qk_strike(qkernel_t *k, uint16_t node, qstrike_t strike);
int qk_run(qkernel_t *k, uint32_t event_budget);

#endif
