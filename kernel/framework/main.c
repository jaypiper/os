#include <common.h>
#include <klib.h>

void *pgalloc(int size);
void pgfree(void *ptr);

volatile static int init_finish = 0;

int main() {
  if(cpu_current() == 0){
    uarths_init();
    printf("Hello OS!\n");
    ioe_init();
    cte_init(os->trap);
    os->init();
    vme_init(pgalloc, pgfree);
    __sync_synchronize();
    init_satp();
    os->test();
    printf("os init finished\n");
    init_finish = 1;
  } else{
    while(!init_finish) ;
    __sync_synchronize();
    cte_init(NULL); // set mtvec
    init_satp();
  }
  os->run();
  return 1;
}
