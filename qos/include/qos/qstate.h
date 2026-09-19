#ifndef QOS_QSTATE_H
#define QOS_QSTATE_H
#include <stdint.h>

typedef enum {
    Q_NEG = 0u,          /* 00 : peak negative tension */
    Q_PRIMED_ZERO = 1u,  /* 01 : active zero, upward momentum */
    Q_DECAY_ZERO = 2u,   /* 10 : active zero, downward momentum */
    Q_POS = 3u           /* 11 : peak positive tension */
} qstate_t;

typedef enum {
    Q_STRIKE_NEG = -1,
    Q_STRIKE_POS = 1
} qstrike_t;

static inline uint8_t qstate_code(qstate_t s) { return ((uint8_t)s) & 0x3u; }
static inline int qstate_magnitude(qstate_t s) {
    return s == Q_POS ? 1 : s == Q_NEG ? -1 : 0;
}
static inline int qstate_direction(qstate_t s) {
    return s == Q_PRIMED_ZERO ? 1 : s == Q_DECAY_ZERO ? -1 : 0;
}

/* Canonical Q-OS v0.1 temporal-friction algebra.
   Positive strike:
     NEG -> PRIMED -> POS, POS saturates.
     DECAY + positive reverses zero momentum -> PRIMED.
   Negative strike is the exact mirror. */
static inline qstate_t q_apply(qstate_t s, qstrike_t strike) {
    if (strike == Q_STRIKE_POS) {
        switch (s) {
            case Q_NEG:         return Q_PRIMED_ZERO;
            case Q_PRIMED_ZERO: return Q_POS;
            case Q_DECAY_ZERO:  return Q_PRIMED_ZERO;
            case Q_POS:         return Q_POS;
        }
    } else {
        switch (s) {
            case Q_NEG:         return Q_NEG;
            case Q_PRIMED_ZERO: return Q_DECAY_ZERO;
            case Q_DECAY_ZERO:  return Q_NEG;
            case Q_POS:         return Q_DECAY_ZERO;
        }
    }
    return s;
}

static inline const char *qstate_name(qstate_t s) {
    switch (s) {
        case Q_NEG: return "-";
        case Q_PRIMED_ZERO: return "-+";
        case Q_DECAY_ZERO: return "+-";
        case Q_POS: return "+";
    }
    return "?";
}

#endif
