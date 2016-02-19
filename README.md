# SPConcurrency

SPConcurrency is a small iOS framework written to provide concurrent programming primitives and data structures that can be used in real-time programming contexts, e.g. audio-processing (CoreAudio, Audio Units), etc.

The library contains:

  - **concurrency primitives** -- atomic operations, barriers, markable pointers, etc.
  - **memory reclamation** -- a fixed-size lock-free memory reclamation scheme adapted from the corrected version of Valois's algorithm (Michael & Scott)
  - **data structures**:
    - **lock-free list**
    - **lock-free priority queue** -- a corrected and improved version of Sundell & Tsigas's queue (--the original version contained numerous data race issues)
    - **wait-free ring buffer**
  - **message-passing**:
    - **message queue** intended to execute blocks on the main thread
    - **lock-free real-time queue** intended for real-time processing, e.g. during an audio callback
    - **real-time scheduler** providing time-based event scheduling

## Examples:

```objc
struct test_elem_t {
    SPCPriorityQueueKey     key;
    void                  *data;
};

@property (nonatomic) SPCPriorityQueue pqueue;

const size_t kDefaultQueueSize = 4096;
```

...


```objc
SPCPriorityQueueInit(&_pqueue, kDefaultQueueSize)


// Insert an element.
test_elem_t oneElem = {
  .key  = 1.0,
  .data = (void *)(0x1)
};
SPCPriorityQueueInsertElement(&_pqueue, oneElem.key, oneElem.data)


// Get the minimum element out of the queue.
test_elem_t retrieveOneElem;
retrieveOneElem.data = SPCPriorityQueueExtractMinimumElement(&_pqueue, &retrieveOneElem.key);
  
  
SPCPriorityQueueDispose(&_pqueue);
```
