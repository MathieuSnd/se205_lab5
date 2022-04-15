#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "circular_buffer.h"
#include "protected_buffer.h"
#include "utils.h"



// Initialise the protected buffer structure above. 
protected_buffer_t * cond_protected_buffer_init(int length) {
  protected_buffer_t * b;
  b = (protected_buffer_t *)malloc(sizeof(protected_buffer_t));
  b->buffer = circular_buffer_init(length);

  // Initialize the synchronization components
  pthread_mutex_init(&b->mut, NULL);
  pthread_cond_init(&b->cond_full, NULL);
  pthread_cond_init(&b->cond_empty, NULL);

  return b;
}

// Extract an element from buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void * cond_protected_buffer_get(protected_buffer_t * b){
  void * d;
  
  // Enter mutual exclusion

  pthread_mutex_lock(&b->mut);
  
  
  while(!circular_buffer_size(b->buffer))
    pthread_cond_wait(&b->cond_full, &b->mut);
  // Wait until there is a full slot to get data from the unprotected
  // circular buffer (circular_buffer_get).

  d = circular_buffer_get(b->buffer);
  print_task_activity ("get", d);
  
  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_signal(&b->cond_empty);

  // Leave mutual exclusion
  pthread_mutex_unlock(&b->mut);
  
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void cond_protected_buffer_put(protected_buffer_t * b, void * d){

  // Enter mutual exclusion
  
  pthread_mutex_lock(&b->mut);  
  

  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put).
  
  while(b->buffer->size == b->buffer->max_size)
    pthread_cond_wait(&b->cond_empty, &b->mut);

  circular_buffer_put(b->buffer, d);
  print_task_activity ("put", d);
  
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_signal(&b->cond_full);

  // Leave mutual exclusion
  pthread_mutex_unlock(&b->mut);
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, return NULL. Otherwise, return the element.
void * cond_protected_buffer_remove(protected_buffer_t * b){
  void * d;
  
  pthread_mutex_lock(&b->mut);

  d = circular_buffer_get(b->buffer);
  print_task_activity ("remove", d);


  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_signal(&b->cond_empty);

  pthread_mutex_unlock(&b->mut);
  
  return d;
}


// Insert an element into buffer. If the attempted operation is
// not possible immedidately, return 0. Otherwise, return 1.
int cond_protected_buffer_add(protected_buffer_t * b, void * d){
  int done;
  
  // Enter mutual exclusion
  pthread_mutex_lock(&b->mut);

  done = circular_buffer_put(b->buffer, d);
  if (!done) d = NULL;
  print_task_activity ("add", d);

  
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_signal(&b->cond_full);

  
  // Leave mutual exclusion
  pthread_mutex_unlock(&b->mut);

  return done;
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return the element if
// successful. Otherwise, return NULL.
void * cond_protected_buffer_poll(protected_buffer_t * b, struct timespec *abstime){
  void * d = NULL;

  // Enter mutual exclusion
  pthread_mutex_lock(&b->mut);
  
  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put) but waits no longer than
  // the given timeout.

  if(!b->buffer->size) {
    // time is absolute, therefore we don't have to worry
    // about the preceding mutex locking time
    int r = pthread_cond_timedwait(&b->cond_full, &b->mut, abstime);

    if(r != ETIMEDOUT)
      d = circular_buffer_get(b->buffer);
  }
  else
    d = circular_buffer_get(b->buffer);
    
  print_task_activity ("poll", d);
  
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)

  pthread_cond_signal(&b->cond_empty);

  // Leave mutual exclusion
  pthread_mutex_unlock(&b->mut);

  return d;
}

// Insert an element into buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return 0 if not
// successful. Otherwise, return 1.
int cond_protected_buffer_offer(protected_buffer_t * b, void * d, struct timespec * abstime){
  int done = 0;

  // Enter mutual exclusion
  pthread_mutex_lock(&b->mut);


  if(b->buffer->size == b->buffer->max_size) {
    int r = pthread_cond_timedwait(&b->cond_empty, &b->mut, abstime);

    if(r != ETIMEDOUT) {
      assert(r == 0);
      done = circular_buffer_put(b->buffer, d);
    }
  }
  else
      done = circular_buffer_put(b->buffer, d);


  if(!done)
    d = NULL;
  print_task_activity ("offer", d);

  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  
  

  pthread_cond_signal(&b->cond_full);
    
  // Leave mutual exclusion
  pthread_mutex_unlock(&b->mut);

  return done;
}
