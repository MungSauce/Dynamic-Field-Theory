#include <stdio.h>
#include "qos/qbin.h"
#include "../kernel/boot_program.h"
#define CHECK(x) do{ if(!(x)){ printf("FAIL line %d: %s\n",__LINE__,#x); return 1;} }while(0)
int main(void){
    qkernel_t k; qbin_result_t r; qk_init(&k);
    int rc=qbin_execute(&k,qos_boot_program,qos_boot_program_len,&r);
    CHECK(rc==0); CHECK(r.expectations==3); CHECK(r.failed_expectations==0);
    CHECK(qk_get(&k,0)==Q_POS); CHECK(qk_get(&k,1)==Q_PRIMED_ZERO); CHECK(qk_get(&k,2)==Q_DECAY_ZERO);
    puts("QOS_QBIN_CONFORMANCE=PASS");
    return 0;
}
