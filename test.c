
#include "rpa_queue.h"
#include <stdio.h>

bool test()
{
  rpa_queue_t * queue = NULL;
  uint32_t queue_capacity = 2;
  if (!rpa_queue_create(&queue, queue_capacity)) {
    printf("failed to create\n");
    return false;
  }
  if (!rpa_queue_push(queue, "item 1")) {
    printf("failed to push item\n");
    return false;
  }
  if (!rpa_queue_trypush(queue, "item 2")) {
    printf("failed to trypush item\n");
    return false;
  }
  if (rpa_queue_trypush(queue, "item 3")) {
    printf("item 3 accepted when it should have blocked\n");
    return false;
  }
  char * data;
  if (!rpa_queue_pop(queue, (void**) &data)) {
    printf("pop failed\n");
    return false;
  }
  printf("popped: %s\n", data);
  if (!rpa_queue_trypop(queue, (void**) &data)) {
    printf("trypop failed\n");
    return false;
  }
  printf("popped: %s\n", data);
  if (rpa_queue_trypop(queue, (void**) &data)) {
    printf("pop succeeded when it should have failed\n");
    return false;
  }
  return true;
}

int main()
{
  if (!test()) {
    return 1;
  }
  printf("test successful\n");
  return 0;
}
