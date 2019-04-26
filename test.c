
#include "rpa_queue.h"
#include <stdio.h>

bool test()
{
  rpa_queue_t * queue = NULL;
  uint32_t queue_capacity = 3;
  if (!rpa_queue_create(&queue, queue_capacity)) {
    printf("failed to create");
    return false;
  }
  return true;
}

int main()
{
  if (!test()) {
    return 1;
  }
  return 0;
}
