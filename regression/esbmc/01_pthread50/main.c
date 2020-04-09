#include <errno.h>
#include <pthread.h>
#include <assert.h>

int main()
{
  pthread_key_t key;
  int r = pthread_key_create(&key, NULL);
  assert(r == 0);
  pthread_t tid;

  long val = (long)pthread_getspecific(key);
  assert(val == 0);

  val = (long)pthread_getspecific(key);
  assert(val == 0);

  r = pthread_setspecific(key, nondet_bool() ? (void *)16 : NULL);
  assert(r == 0 || r == EINVAL);

  return 0;
}
