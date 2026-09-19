#include "qos/qbin.h"

static int need(size_t p,size_t n,size_t len){ return p+n<=len; }
static uint16_t u16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p){ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }

int qbin_execute(qkernel_t *k,const uint8_t *data,size_t len,qbin_result_t *r){
    size_t p=0;
    r->instructions=r->expectations=r->failed_expectations=0;
    while(p<len){
        uint8_t op=data[p++]; r->instructions++;
        if(op==QBIN_END) return r->failed_expectations?2:0;
        if(op==QBIN_NODE){
            if(!need(p,3,len)) return -10;
            uint16_t id=u16(data+p); qstate_t s=(qstate_t)(data[p+2]&3u);p+=3;
            if(qk_alloc(k,id,s)!=0) return -11;
        } else if(op==QBIN_EDGE){
            if(!need(p,5,len)) return -12;
            uint16_t a=u16(data+p),b=u16(data+p+2);qedge_mode_t m=(qedge_mode_t)data[p+4];p+=5;
            if(m>Q_EDGE_CANCEL||qk_link(k,a,b,m)!=0) return -13;
        } else if(op==QBIN_STRIKE){
            if(!need(p,3,len)) return -14;
            uint16_t id=u16(data+p);int8_t s=(int8_t)data[p+2];p+=3;
            if((s!=1&&s!=-1)||qk_strike(k,id,(qstrike_t)s)!=0) return -15;
        } else if(op==QBIN_SEVER_NODE){
            if(!need(p,2,len)) return -16;
            uint16_t id=u16(data+p);p+=2;if(qk_sever_node(k,id)!=0)return -17;
        } else if(op==QBIN_SEVER_EDGE){
            if(!need(p,4,len)) return -18;
            uint16_t a=u16(data+p),b=u16(data+p+2);p+=4;if(qk_sever_link(k,a,b)!=0)return -19;
        } else if(op==QBIN_SETTLE){
            if(!need(p,4,len)) return -20;
            uint32_t budget=u32(data+p);p+=4;int rc=qk_run(k,budget);if(rc!=0)return -21;
        } else if(op==QBIN_EXPECT){
            if(!need(p,3,len)) return -22;
            uint16_t id=u16(data+p);qstate_t s=(qstate_t)(data[p+2]&3u);p+=3;
            r->expectations++;
            if(!qk_is_allocated(k,id)||qk_get(k,id)!=s)r->failed_expectations++;
        } else return -23;
    }
    return -24;
}
