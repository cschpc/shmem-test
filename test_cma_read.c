/* -----------------------------------------------------------------------------
MIT License

Copyright (c) 2022 CSC HPC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------ */

#include "shmem_tests.h"

/* Borrowed from util-linux-2.13-pre7/schedutils/taskset.c */
static char *cpuset_to_cstr(cpu_set_t *mask, char *str)
{
  char *ptr = str;
  int i, j, entry_made = 0;
  for (i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, mask)) {
      int run = 0; 
      entry_made = 1; 
      for (j = i + 1; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, mask)) run++;
        else break;
      }
      if (!run)
        sprintf(ptr, "%d,", i);
      else if (run == 1) {
        sprintf(ptr, "%d,%d,", i, i + 1);
        i++; 
      } else {
        sprintf(ptr, "%d-%d,", i, i + run);
        i += run;
      }
      while (*ptr != 0) ptr++;
    }
  }
  ptr -= entry_made;
  *ptr = 0;
  return(str);
}

double mysecond()
{
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp); 
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

int main(int argc, char** argv)
{

  double times[NTIMES];
  double avgtime = 0;
  double mintime = FLT_MAX;
  double maxtime = 0;

  struct iovec local[1];
  struct iovec remote[1];

  int pipefd[2];

  if (-1 == pipe(pipefd)) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  cpu_set_t coremask;
  char clbuf[7 * CPU_SETSIZE];
  sched_getaffinity(0, sizeof(coremask), &coremask);
  memset(clbuf, 0, sizeof(clbuf));
  cpuset_to_cstr(&coremask, clbuf);
  printf("Original affinity: %s\n", clbuf);
  fflush(stdout);

  pid_t cpid = fork();

  // child
  if (0 == cpid) {
    close(pipefd[1]);

    char clbuf[7 * CPU_SETSIZE];
    int ncpus = CPU_COUNT(&coremask);
    CPU_ZERO(&coremask);
    CPU_SET(ncpus-1, &coremask);
    sched_setaffinity(0, sizeof(coremask), &coremask); 
    memset(clbuf, 0, sizeof(clbuf));
    cpuset_to_cstr(&coremask, clbuf);
    printf("Affinity in child: %s\n", clbuf);
    fflush(stdout);

    double *a;
    double *b = (double *) malloc(BUF_SIZE * sizeof(double));

    // invalidate data
    for (size_t i=0; i < BUF_SIZE; i++) {
       b[i] = -1;
    }

    // synchronize with parent
    if (read(pipefd[0], &a, sizeof(double *)) == 0)
        printf("child didn't get data\n");
    char dummy;
    if (read(pipefd[0], &dummy, 1) != 0)
        printf("child didn't get EOF\n");
    
    // read data
    pid_t ppid = getppid();
    local[0].iov_base = b;
    local[0].iov_len = BUF_SIZE * sizeof(double);
    remote[0].iov_base = a;
    remote[0].iov_len = BUF_SIZE * sizeof(double);

    size_t nread;
    for (int k=0; k < NTIMES; k++)
      {
        times[k] = mysecond();
        nread = process_vm_readv(ppid, local, 1, remote, 1, 0);
        times[k] = mysecond() - times[k];
      }
    if (-1 == nread) {
      printf("Error in read %d\n", errno);
    }

    // Use Kahan's algorithm for summation
    double checksum = b[0];
    double tmp_c = 0.0;
    for (size_t i=1; i < BUF_SIZE; i++) {
      double y = b[i] - tmp_c;
      double t = checksum + y;
      tmp_c = (t - checksum) - y;
      checksum = t;
    }
    checksum /= (double) BUF_SIZE;

    for (int k=1; k < NTIMES; k++) /* note -- skip first iteration */
      {
        avgtime = avgtime + times[k];
        mintime = MIN(mintime, times[k]);
        maxtime = MAX(maxtime, times[k]);
      }
    
    size_t bytes = BW_CONVENTION * sizeof(double) * BUF_SIZE;
  
    printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
    avgtime = avgtime / (double)(NTIMES-1);

    printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", "vm_readv",
           1.0E-06 * bytes/mintime,
           avgtime,
           mintime,
           maxtime);

    printf(HLINE);
    printf("check: %f %f\n", checksum, 0.5*(BUF_SIZE-1));

  // parent
  } else {
    close(pipefd[0]);

    char clbuf[7 * CPU_SETSIZE];
    cpu_set_t coremask;
    CPU_ZERO(&coremask);
    CPU_SET(0, &coremask);
    sched_setaffinity(0, sizeof(coremask), &coremask); 
    memset(clbuf, 0, sizeof(clbuf));
    cpuset_to_cstr(&coremask, clbuf);
    printf("Affinity in parent: %s\n", clbuf);
    fflush(stdout);

    double *a = (double *) malloc(BUF_SIZE * sizeof(double));

    for (size_t i=0; i < BUF_SIZE; i++)
      a[i] = i;

    if (write(pipefd[1], &a, sizeof(double *)) == 0)
        printf("Parent couldn't write data\n");

    // synchronize with child
    close(pipefd[1]);
    
    wait(NULL);

  }
}
