#include <sys/stat.h>
#include <unistd.h>

#include <mdtest.h>

#include <io500-phase.h>
#include <phase_mdtest.h>

opt_mdtest_easy mdtest_easy_o;

static ini_option_t option[] = {
  {"API", "The API to be used", 0, INI_STRING, NULL, & mdtest_easy_o.g.api},
  {"n", "Files per proc", 0, INI_UINT64, "1000000", & mdtest_easy_o.g.files_per_proc},
  {"run", "Run this phase", 0, INI_BOOL, "TRUE", & mdtest_easy_o.g.run},
  {NULL} };

static void validate(void){

}

static void cleanup(void){
  if( ! opt.dry_run && opt.rank == 0){
    char filename[PATH_MAX];
    sprintf(filename, "%s/mdtest-easy.stonewall", opt.resdir);
    unlink(filename);
    u_purge_datadir("mdtest-easy");
  }
}

void mdtest_easy_add_params(u_argv_t * argv){
  opt_mdtest_easy d = mdtest_easy_o;

  u_argv_push(argv, "./mdtest");
  if(opt.io_buffers_on_gpu){
    u_argv_push(argv, "--allocateBufferOnGPU");
  }
  u_argv_push(argv, "-n");
  u_argv_push_printf(argv, "%"PRIu64, d.g.files_per_proc);
  u_argv_push(argv, "-u");
  u_argv_push(argv, "-L");
  u_argv_push(argv, "-F");
  u_argv_push(argv, "-P");
  u_argv_push(argv, "-G");
  int hash = u_phase_unique_random_number("mdtest-easy");
  u_argv_push_printf(argv, "%d", hash);
  u_argv_push(argv, "-N");
  u_argv_push(argv, "1");
  u_argv_push(argv, "-d");
  u_argv_push_printf(argv, "%s/mdtest-easy", opt.datadir);
  u_argv_push(argv, "-x");
  u_argv_push_printf(argv, "%s/mdtest-easy.stonewall", opt.resdir);
}

u_phase_t p_mdtest_easy = {
  "mdtest-easy",
  IO500_PHASE_DUMMY,
  option,
  validate,
  NULL,
  .cleanup = cleanup,
};
