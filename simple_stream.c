#include "shmem_tests.h"

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

  double *a = (double *) malloc(BUF_SIZE * sizeof(double));
  double *b = (double *) malloc(BUF_SIZE * sizeof(double));
  
  for (int i=0; i < BUF_SIZE; i++) {
    a[i] = i;
    b[i] = -1.0;
  }

  for (int k=0; k < NTIMES; k++)
    {
      times[k] = mysecond();

      // copy data
#ifdef USE_AVX      
      #pragma omp simd
      for (int i=0; i < BUF_SIZE; i++)
        b[i] = a[i];
#else
      memcpy(b, a, sizeof(double) * BUF_SIZE);
#endif      
      
      times[k] = mysecond() - times[k];
    }
  
  for (int k=1; k < NTIMES; k++) /* note -- skip first iteration */
    {
      avgtime = avgtime + times[k];
      mintime = MIN(mintime, times[k]);
      maxtime = MAX(maxtime, times[k]);
    }

  size_t bytes = BW_CONVENTION * sizeof(double) * BUF_SIZE;
  
  printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
  avgtime = avgtime / (double)(NTIMES-1);
  
  printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", "simple copy",
         1.0E-06 * bytes/mintime,
         avgtime,
         mintime,
         maxtime);

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
  
  printf(HLINE);
  printf("check: %f %f\n", checksum, 0.5*(BUF_SIZE-1));

}
