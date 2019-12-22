#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <mpi.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include <io500-util.h>
#include <io500-phase.h>

#include <phase-definitions.h>

static char const * io500_phase_str[IO500_SCORE_LAST] = {
  "NO SCORE",
  "MD",
  "BW"};

static void init_dirs(void){
  // load general IO backend for data dir
  aiori_initialize(NULL);
  opt.aiori = aiori_select(opt.api);
  if(opt.aiori == NULL){
    FATAL("Could not load AIORI backend for %s\n", opt.api);
  }

  char buffer[30];

  if(opt.rank == 0){
    struct tm* tm_info;
    time_t timer;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 30, "%Y.%m.%d-%H.%M.%S", tm_info);
  }
  UMPI_CHECK(MPI_Bcast(buffer, 30, MPI_CHAR, 0, MPI_COMM_WORLD));

  char resdir[2048];
  if(opt.timestamp_resdir){
    sprintf(resdir, "%s/%s", opt.resdir, buffer);
    opt.resdir = strdup(resdir);
  }

  if(opt.timestamp_datadir){
    sprintf(resdir, "%s/%s", opt.datadir, buffer);
    opt.datadir = strdup(resdir);
  }

  if(opt.rank == 0){
    PRINT_PAIR("result-dir", "%s\n", opt.resdir);
    u_create_dir_recursive(opt.resdir, "POSIX");

    u_create_datadir("");
  }
}

int main(int argc, char ** argv){
  ini_section_t ** cfg = u_options();

  MPI_Init(& argc, & argv);
  MPI_Comm_rank(MPI_COMM_WORLD, & opt.rank);
  MPI_Comm_size(MPI_COMM_WORLD, & opt.mpi_size);

  init_IOR_Param_t(& opt.aiori_params);

  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0){
    help:
    if(opt.rank != 0){
      MPI_Finalize();
      exit(0);
    }
    r0printf("Synopsis: %s <INI file> [-v=<verbosity level>] [--dry-run] [--cleanup] [--config-hash]\n\n", argv[0]);
    r0printf("--dry-run will show the executed IO benchmark arguments but not run them (It will run drop caches, though, if enabled)\n");
    r0printf("--cleanup will run the delete phases of the benchmark useful to get rid of a partially executed benchmark\n");
    r0printf("--config-hash Compute the configuration hash\n\n");
    r0printf("--verify to verify that the output hasn't been modified accidentially; call like: io500 test.ini --verify test.out\n\n");

    r0printf("Supported and current values of the ini file:\n");
    u_ini_print_values(cfg);
    MPI_Finalize();
    exit(0);
  }

  int verbosity_override = -1;
  int print_help = 0;

  int config_hash_only = 0;
  int cleanup_only = 0;
  int verify_only = 0;
  if(argc > 2){
    for(int i = 2; i < argc; i++){
      if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ){
        print_help = 1;
      }else if(strncmp(argv[i], "-v=", 3) == 0){
        verbosity_override = atoi(argv[i]+3);
        opt.verbosity = verbosity_override;
      }else if(strcmp(argv[i], "--dry-run") == 0 ){
        opt.dry_run = 1;
      }else if(strcmp(argv[i], "--cleanup") == 0 ){
        cleanup_only = 1;
      }else if(strcmp(argv[i], "--config-hash") == 0 ){
        config_hash_only = 1;
      }else if(strcmp(argv[i], "--verify") == 0 ){
        verify_only = 1;
        break;
      }else{
        FATAL("Unknown option: %s\n", argv[i]);
      }
    }
  }


  u_ini_parse_file(argv[1], cfg, NULL);
  if(verbosity_override > -1){
    opt.verbosity = verbosity_override;
  }
  if(print_help){
    goto help;
  }

  PRINT_PAIR("version", "%s\n", VERSION);

  if(verify_only){
    if(argc == 3){
      FATAL("--verify option requires the output file as last parameter!");
    }
    if(verbosity_override == -1){
      opt.verbosity = 0;
    }
    u_verify_result_files(cfg, argv[argc-1]);
    exit(0);
  }

  if(opt.rank == 0){
    PRINT_PAIR_HEADER("config-hash");
    uint32_t hash = u_ini_gen_hash(cfg);
    u_hash_print(stdout, hash);
    printf("\n");
  }

  if(config_hash_only){
    MPI_Finalize();
    exit(0);
  }

  init_dirs();

  MPI_Barrier(MPI_COMM_WORLD);
  if(opt.verbosity > 0 && opt.rank == 0){
    printf("; START ");
    u_print_timestamp();
    printf("\n");
  }

  for(int i=0; i < IO500_PHASES; i++){
    phases[i]->validate();
  }
  if(opt.rank == 0){
    printf("\n");
  }

  // manage a hash for the scores
  uint32_t score_hash = 0;
  u_hash_update_key_val(& score_hash, "version", VERSION);

  for(int i=0; i < IO500_PHASES; i++){
    if(! phases[i]->run) continue;
    if( cleanup_only && phases[i]->type != IO500_PHASE_REMOVE ) continue;

    if(opt.drop_caches && phases[i]->type != IO500_PHASE_DUMMY){
      DEBUG_INFO("Dropping cache\n");
      if(opt.rank == 0)
        u_call_cmd("LANG=C free -m");
      u_call_cmd(opt.drop_caches_cmd);
      if(opt.rank == 0)
        u_call_cmd("LANG=C free -m");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if(opt.rank == 0){
      printf("\n[%s]\n", phases[i]->name);
      if(opt.verbosity > 0){
        PRINT_PAIR_HEADER("t_start");
        u_print_timestamp();
        printf("\n");
      }
    }

    double start = GetTimeStamp();
    double score = phases[i]->run();
    if(phases[i]->group > IO500_NO_SCORE){
      if(opt.rank == 0){
        PRINT_PAIR("score", "%f\n", score);
      }
      u_hash_update_key_val_dbl(& score_hash, phases[i]->name, score);
    }
    phases[i]->score = score;

    double runtime = GetTimeStamp() - start;
    // This is an additional sanity check
    if( phases[i]->verify_stonewall && opt.rank == 0){
      if(runtime < opt.stonewall && ! opt.dry_run){
        opt.is_valid_run = 0;
        ERROR("Runtime of phase (%f) is below stonewall time. This shouldn't happen!\n", runtime);
      }
    }

    if(opt.verbosity > 0 && opt.rank == 0){
      PRINT_PAIR("t_delta", "%.4f\n", runtime);
      PRINT_PAIR_HEADER("t_end");
      u_print_timestamp();
      printf("\n");
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  if(opt.rank == 0){
    // compute the overall score
    printf("\n[SCORE]\n");
    double overall_score = 0;

    for(int g=1; g < IO500_SCORE_LAST; g++){
      char score_string[2048];
      char *p = score_string;
      double score = 0;
      int numbers = 0;
      p += sprintf(p, " %s = (", io500_phase_str[g]);
      for(int i=0; i < IO500_PHASES; i++){
        if(phases[i]->group == g){
          double t = phases[i]->score;
          score += t*t;
          if(numbers > 0)
            p += sprintf(p, " + ");
          numbers++;
          p += sprintf(p, "(%.3f*%.3f)", t, t);
        }
      }
      DEBUG_INFO("%s)^%f\n", score_string, 1.0/numbers);
      score = pow(score, 1.0/numbers);
      PRINT_PAIR(io500_phase_str[g], "%f\n", score);
      u_hash_update_key_val_dbl(& score_hash, io500_phase_str[g], score);

      overall_score += score * score;
    }
    PRINT_PAIR("SCORE", "%f %s\n", sqrt(overall_score), opt.is_valid_run ? "" : " [INVALID]");
    u_hash_update_key_val_dbl(& score_hash, "SCORE", overall_score);
    if( ! opt.is_valid_run ){
      u_hash_update_key_val(& score_hash, "valid", "NO");
    }
    PRINT_PAIR("hash", "%X\n", (int) score_hash);
  }

  for(int i=0; i < IO500_PHASES; i++){
    if(phases[i]->cleanup)
      phases[i]->cleanup();
  }

  u_purge_datadir("");

  if(opt.rank == 0 && opt.verbosity > 0){
    printf("; END ");
    u_print_timestamp();
    printf("\n");
  }

  MPI_Finalize();
  return 0;
}
