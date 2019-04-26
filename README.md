# rpa_queue
Simple Blocking Queues in C

Features:
 - just one header and one C file
 - only depends on pthreads
 - support timeouts on push/pop operations
 - proven implementation (Apache apr_queue)

This is Apache apr_queue with the following modifications:
 - apr dependencies stripped off (using malloc instead of apr_pool, etc.)
 - add timeout versions
