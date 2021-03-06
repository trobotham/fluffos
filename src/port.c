#include "std.h"
#include "port.h"
#include "file_incl.h"
#include "network_incl.h"
#include <unistd.h>
#include <sys/resource.h>

#include <random>

// Returns a pseudo-random number in the range 0 .. n-1
int64_t random_number(int64_t n)
{
  static bool called = 0;
  static std::mt19937_64 engine;

  if(!called) {
    std::random_device rd;
    engine.seed(rd());
    called = 1;
  }

  std::uniform_int_distribution<int64_t> dist(0, n-1);
  return dist(engine);
}

/*
 * The function time() can't really be trusted to return an integer.
 * But MudOS uses the 'current_time', which is an integer number
 * of seconds. To make this more portable, the following functions
 * should be defined in such a way as to return the number of seconds since
 * some chosen year. The old behaviour of time(), is to return the number
 * of seconds since 1970.
 */

long get_current_time()
{
  return time(0l);    /* Just use the old time() for now */
}

const char *time_string(time_t t)
{
  const char *res = ctime(&t);
  if (!res) {
    res = "ctime failed";
  }
  return res;
}

/*
 * Get a microsecond clock sample.
 */
void
get_usec_clock(long *sec, long *usec)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  *sec = tv.tv_sec;
  *usec = tv.tv_usec;
}

long get_cpu_times(unsigned long *secs, unsigned long *usecs)
{
  struct rusage rus;

  if (getrusage(RUSAGE_SELF, &rus) < 0) {
    return 0;
  }
  *secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
  *usecs = rus.ru_utime.tv_usec + rus.ru_stime.tv_usec;
  return 1;
}

/* return the current working directory */
char *
get_current_dir(char *buf, int limit)
{
  return getcwd(buf, limit);  /* POSIX */
}

#ifdef MMAP_SBRK
void *sbrkx(long size)
{
  static void *end = 0;
  static unsigned long tsize = 0;
  void *tmp, *result;
  long long newsize;
  if (!end) {
    tmp = mmap((void *)0x41000000, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    tsize = 4096;
    end = (void *)0x41000000;
    if (tmp != end) {
      return NULL;
    }
  }

  newsize = (long)end + size;
  result = end;
  end = newsize;
  newsize &= 0xFFFFF000;
  newsize += 0x1000;
  newsize -= 0x41000000;

  tmp = mremap((void *)0x41000000, tsize, newsize, 0);

  if (tmp != (void *) 0x41000000) {
    return NULL;
  }

  tsize = newsize;
  return result;
}

#else

void *sbrkx(long size)
{
#ifndef MINGW
  return sbrk(size);
#endif
}
#endif
