#include <common.h>
#include <klib.h>

void *pgalloc(int size);
void pgfree(void *ptr);

int main() {
  ioe_init();
  cte_init(os->trap);
  os->init();
  vme_init(pgalloc, pgfree);
  mpe_init(os->run);
  return 1;
}
