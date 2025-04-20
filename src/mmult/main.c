#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sched.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "impl/ref.h"
#include "impl/naive.h"
#include "impl/opt.h"
#include "impl/vec.h"
#include "impl/para.h"
#include "common/types.h"
#include "common/macros.h"
#include "include/types.h"

int M_val      = 16;
int N_val      = 12;
int P_val      = 8;
int b_val      = 16;
int nruns      = 10000;
int nstdevs    = 3;
int nthreads   = 1;
int cpu        = 0;
bool load_data = false;

static void load_file(const char* fn, byte* buf, size_t n) {
    FILE* f = fopen(fn, "rb");
    if (!f) { perror(fn); exit(1); }
    if (fread(buf,1,n,f) != n) { fprintf(stderr,"short read %s\n",fn); exit(1); }
    fclose(f);
}

static void wait_for_file_size(const char* fn, size_t want, int to, int iv) {
    int w = 0;
    while (w < to) {
        FILE* f = fopen(fn, "rb");
        if (f) {
            fseek(f,0,SEEK_END);
            if ((size_t)ftell(f) >= want) { fclose(f); return; }
            fclose(f);
        }
        sleep(iv); w += iv;
    }
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    void* (*impl_naive)(void*) = impl_scalar_naive;
    void* (*impl_opt  )(void*) = impl_scalar_opt;
    void* (*impl_vec  )(void*) = impl_vector;
    void* (*impl_para )(void*) = impl_parallel;
    void* (*impl       )(void*) = NULL;
    const char* impl_str = NULL;
    bool help = false;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i],"-i")||!strcmp(argv[i],"--impl")) {
            assert(++i<argc);
            if (!strcmp(argv[i],"naive")) impl = impl_naive,  impl_str = "scalar_naive";
            else if (!strcmp(argv[i],"opt"))  impl = impl_opt,    impl_str = "scalar_opt";
            else if (!strcmp(argv[i],"vec"))  impl = impl_vec,    impl_str = "vectorized";
            else if (!strcmp(argv[i],"para")) impl = impl_para,   impl_str = "parallelized";
            else                              impl = NULL,         impl_str = "unknown";
        }
        else if (!strcmp(argv[i],"--M"))      { assert(++i<argc); M_val     = atoi(argv[i]); }
        else if (!strcmp(argv[i],"--N"))      { assert(++i<argc); N_val     = atoi(argv[i]); }
        else if (!strcmp(argv[i],"--P"))      { assert(++i<argc); P_val     = atoi(argv[i]); }
        else if (!strcmp(argv[i],"--b"))      { assert(++i<argc); b_val     = atoi(argv[i]); }
        else if (!strcmp(argv[i],"--load"))   { load_data = true; }
        else if (!strcmp(argv[i],"--nruns"))  { assert(++i<argc); nruns     = atoi(argv[i]); }
        else if (!strcmp(argv[i],"--nstdevs")){ assert(++i<argc); nstdevs   = atoi(argv[i]); }
        else if (!strcmp(argv[i],"-n")||!strcmp(argv[i],"--nthreads")) {
                                              assert(++i<argc); nthreads  = atoi(argv[i]); }
        else if (!strcmp(argv[i],"-c")||!strcmp(argv[i],"--cpu")) {
                                              assert(++i<argc); cpu       = atoi(argv[i]); }
        else if (!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")) {
                                              help = true; }
        else { help = true; }
    }

    if (help || !impl) {
        if (!help) fprintf(stderr,"Unknown impl \"%s\"\n", impl_str);
        fprintf(stderr,"Usage: %s -i {naive|opt|vec|para} [--M m] [--N n] [--P p] "
                       "[--b block] [--nruns r] [--nstdevs s]\n", argv[0]);
        return help ? 0 : 1;
    }

    printf("Setting up schedulers & affinity...\n");
    { int nl = -20; do { errno=0; nice(nl); } while(errno && nl++); }
#if !defined(__APPLE__)
    {
      pid_t pid = 0;
      struct sched_param p = { .sched_priority = sched_get_priority_max(SCHED_FIFO) };
      sched_setscheduler(pid, SCHED_FIFO, &p);
      cpu_set_t m; CPU_ZERO(&m);
      for(int i=0;i<nthreads;i++) CPU_SET((cpu+i)%nthreads,&m);
      sched_setaffinity(pid,sizeof(m),&m);
    }
#endif
    printf("\n");

    __DECLARE_STATS(nruns, nstdevs);
    srand(0xdeadbeef);

    size_t szA = (size_t)M_val*N_val*sizeof(float);
    size_t szB = (size_t)N_val*P_val*sizeof(float);
    size_t szR = (size_t)M_val*P_val*sizeof(float);

    byte* A  = __ALLOC_INIT_DATA(byte, szA);
    byte* B  = __ALLOC_INIT_DATA(byte, szB);
    byte* Rr = __ALLOC_INIT_DATA(byte, szR+4);
    byte* Rd = __ALLOC_DATA     (byte, szR+4);

    if (load_data) {
      load_file("A.bin", A, szA);
      load_file("B.bin", B, szB);
    }

    __SET_GUARD(Rr, szR);
    __SET_GUARD(Rd, szR);

    args_t ref_args = {
      .A        = (float*)A,
      .B        = (float*)B,
      .R        = (float*)Rr,
      .M        = M_val,
      .N        = N_val,
      .P        = P_val,
      .cpu      = cpu,
      .nthreads = nthreads,
      .b        = b_val      // <-- ensure block size gets passed
    };
    impl_ref(&ref_args);

    args_t run_args = ref_args;
    run_args.R = (float*)Rd;

    printf("Running \"%s\" implementation:\n", impl_str);
    printf("  * Invoking %d runs ... ", nruns);
    for (int i = 0; i < nruns; i++) {
      __SET_START_TIME();
      impl(&run_args);
      __SET_END_TIME();
      runtimes[i] = __CALC_RUNTIME();
    }
    printf("Finished\n\n");

    printf("  * Verifying results ... ");
    bool match = __CHECK_MATCH(Rr, Rd, szR);
    if (!match) {
  size_t total = M_val * P_val;
  for (size_t idx = 0; idx < total; ++idx) {
    float refv = ((float*)Rr)[idx];
    float optv = ((float*)Rd)[idx];
    if (fabsf(refv - optv) > 1e-6f) {
      size_t i = idx / P_val, j = idx % P_val;
      printf("Mismatch at [%zu,%zu] (flat %zu): ref=%.7f  opt=%.7f\n",
             i, j, idx, refv, optv);
      break;
    }
  }
}

    bool guard = __CHECK_GUARD(Rd, szR);
    if      (match && guard)    printf("Success\n");
    else if (!match && guard)   printf("Fail (mismatch)\n");
    else if ( match && !guard)  printf("Fail (overrun)\n");
    else                         printf("Fail (mismatch+overrun)\n");

    // dump out for python compare if you like
    FILE* f = fopen("computed.bin","wb");
    fwrite(Rd, sizeof(float), szR/sizeof(float), f);
    fclose(f);
    wait_for_file_size("computed.bin", szR, 10, 1);

    printf("  * Running statistics (> %d stdevs removed):\n", nstdevs);
    for(int i=0;i<nruns;i++) runtimes_mask[i] = true;
    int round = 0, masked;
    uint64_t avg = 0;
    do {
      round++;
      uint64_t mn=UINT64_MAX, mx=0, sum=0, cnt=0;
      for(int i=0;i<nruns;i++) if(runtimes_mask[i]) {
        uint64_t t = runtimes[i];
        mn = t<mn ? t : mn;
        mx = t>mx ? t : mx;
        sum += t; cnt++;
      }
      avg = sum/cnt;
      double var = 0;
      for(int i=0;i<nruns;i++) if(runtimes_mask[i]) {
        double d = (double)runtimes[i] - avg;
        var += d*d;
      }
      double st = sqrt(var/cnt);
      masked = 0;
      for(int i=0;i<nruns;i++) if(runtimes_mask[i]) {
        if (fabs((double)runtimes[i] - avg) > nstdevs*st) {
          runtimes_mask[i] = false; masked++;
        }
      }
      printf("    + Round %d: avg=%" PRIu64 " ns, stdev=%.2f, masked=%d\n",
             round, avg, st, masked);
    } while(masked>0);
    printf("  * Final runtime: %" PRIu64 " ns\n\n", avg);

    char fn[64];
    snprintf(fn,sizeof(fn),"%s_runtimes.csv", impl_str);
    printf("  * Dumping runtimes to %s\n", fn);
    FILE* c = fopen(fn,"w");
    if (c) {
      fprintf(c,"impl,%s\nnum_runs,%d\nruntimes", impl_str, nruns);
      for(int i=0;i<nruns;i++) fprintf(c,",%" PRIu64, runtimes[i]);
      fprintf(c,"\navg,%" PRIu64 "\n", avg);
      fclose(c);
    }

    free(A); free(B); free(Rr); free(Rd);
    __DESTROY_STATS();
    return 0;
}

