#include <common.h>
#include <klib.h>

void *pgalloc(int size);
void pgfree(void *ptr);

volatile static int init_finish = 0;

void print_logo(){
printf("\
       GCCCCCt CCCCf                          \n\
    CCCC  CCCCCC   CCCCCf                 .-') _                                                         \n\
  CCC:::: ;CC   fGL   CC                 ( OO ) )                                                        \n\
  1tttttt             CC CC.         ,--./ ,--,'      ,--. ,--. ,--.   .---. .-----.   .----.            \n\
CCCCCCCCCt CCCCCCCCCCCCCCCCC,        |   \\ |  |\\  .-')| ,| |  | |  |  /_   |/ ,-.   \\ /  ..  \\           \n\
       .CG CC         CC   CC        |    \\|  | )( OO |(_| |  | | .-') |   |'-'  |  |.  /  \\  .          \n\
CCCCCCCCCG CC         CC   CC        |  .     |/ | `-'|  | |  |_|( OO )|   |   .'  / |  |  '  |          \n\
CC         CCCCCCCCC  CCCC CC        |  |\\    |  ,--. |  | |  | | `-' /|   | .'  /__ '  \\  /  '          \n\
CCCCCCCCCf CC     CC  CC   CC        |  | \\   |  |  '-'  /('  '-'(_.-' |   ||       | \\  `'  /           \n\
 ,,,,,,,:  CC     CC  CC  ;CC        `--'  `--'   `-----'   `-----'    `---'`-------'  `---''           \n\
 CCLLLLfCC CC     CC  CC :CC                  \n\
  CC;   CC CC     CC  CCCCC                   \n\
   ,CCCCCC CC     CC  CCC                     \n\
      CCCG CC    fCC                          \n\
           GG CCCt \n\
");
}



int main() {
  if(cpu_current() == 0){
    ioe_init();
    cte_init(os->trap);
    os->init();
    vme_init(pgalloc, pgfree);
    __sync_synchronize();
    init_satp();
    os->test();
    // print_logo();
    printf("user init finished\n");
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
