#ifndef QOS_QBIN_H
#define QOS_QBIN_H
#include <stddef.h>
#include <stdint.h>
#include "qkernel.h"

enum {
    QBIN_NODE=0x01,
    QBIN_EDGE=0x02,
    QBIN_STRIKE=0x03,
    QBIN_SEVER_NODE=0x04,
    QBIN_SEVER_EDGE=0x05,
    QBIN_SETTLE=0x06,
    QBIN_EXPECT=0x07,
    QBIN_END=0xff
};

typedef struct {
    uint32_t instructions;
    uint32_t expectations;
    uint32_t failed_expectations;
} qbin_result_t;

int qbin_execute(qkernel_t *k,const uint8_t *data,size_t len,qbin_result_t *result);
#endif
