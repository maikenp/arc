#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sys/types.h>
#include <pwd.h>
#include <string>
#include <cstdio>
#include <fstream>
#include <list>
#include <fcntl.h>
#include <signal.h>

#include <arc/FileUtils.h>
#include <arc/Logger.h>
#include <arc/Run.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>
#include <arc/Utils.h>
#include "jobs/users.h"
#include "jobs/states.h"
#include "jobs/commfifo.h"
#include "conf/environment.h"
#include "conf/conf_file.h"
//#include "conf/daemon.h"
#include "files/info_types.h"
#include "files/delete.h"
#include "log/job_log.h"
#include "run/run_redirected.h"
#include "run/run_parallel.h"
#include "jobs/dtr_generator.h"

#include "grid_manager.h"

/* do job cleaning every 2 hours */
#define HARD_JOB_PERIOD 7200

/* cache cleaning every 5 minutes */
#define CACHE_CLEAN_PERIOD 300

/* cache cleaning default timeout */
#define CACHE_CLEAN_TIMEOUT 3600

#define DEFAULT_LOG_FILE "/var/log/arc/grid-manager.log"
#define DEFAULT_PID_FILE "/var/run/grid-manager.pid"

namespace ARex {

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AREX:GM");

static void* cache_func(void* arg) {
  const JobUsers* users = (const JobUsers*)arg;
  JobUser gmuser(users->Env(),getuid(),getgid()); // Cleaning should run under the GM user
  
  // run cache cleaning periodically forever
  for(;;) {
    // go through each user and clean their caches. One user is processed per clean period
    // if this flag is false after all users there are no caches and the thread exits
    bool have_caches = false;

    for (JobUsers::const_iterator cacheuser = users->begin(); cacheuser != users->end(); ++cacheuser) {
      CacheConfig cache_info = cacheuser->CacheParams();
      if (!cache_info.cleanCache()) continue;

      // get the cache dirs
      std::vector<std::string> cache_info_dirs = cache_info.getCacheDirs();
      if (cache_info_dirs.empty()) continue;
      have_caches = true;

      // in arc.conf % of used space is given, but cache-clean uses % of free space
      std::string minfreespace = Arc::tostring(100-cache_info.getCacheMax());
      std::string maxfreespace = Arc::tostring(100-cache_info.getCacheMin());
      std::string cachelifetime = cache_info.getLifeTime();
      std::string logfile = cache_info.getLogFile();

      int h = open(logfile.c_str(), O_WRONLY | O_APPEND);
      if (h < 0) {
        std::string dirname(logfile.substr(0, logfile.rfind('/')));
        if (!dirname.empty() && !Arc::DirCreate(dirname, S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH, true)) {
          logger.msg(Arc::WARNING, "Cannot create directories for log file %s."
              " Messages will be logged to this log", logfile);
        }
        else {
          h = open(logfile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
          if (h < 0) {
            logger.msg(Arc::WARNING, "Cannot open cache log file %s: %s. Cache cleaning"
                " messages will be logged to this log", logfile, Arc::StrError(errno));
          }
        }
      }

      // do cache-clean -h for explanation of options
      std::string cmd = users->Env().nordugrid_libexec_loc() + "/cache-clean";
      cmd += " -m " + minfreespace;
      cmd += " -M " + maxfreespace;
      if (!cachelifetime.empty()) cmd += " -E " + cachelifetime;
      cmd += " -D " + cache_info.getLogLevel();
      std::vector<std::string> cache_dirs;
      for (std::vector<std::string>::iterator i = cache_info_dirs.begin(); i != cache_info_dirs.end(); i++) {
        cmd += " " + (i->substr(0, i->find(" ")));
      }
      logger.msg(Arc::DEBUG, "Running command %s", cmd);
      // use large timeout, as disk scan can take a long time
      // blocks until command finishes or timeout
      int clean_timeout = cache_info.getCleanTimeout();
      if (clean_timeout == 0) clean_timeout = CACHE_CLEAN_TIMEOUT;
      int result = RunRedirected::run(gmuser, "cache-clean", -1, h, h, cmd.c_str(), clean_timeout);
      close(h);
      if (result != 0) {
        if (result == -1) logger.msg(Arc::ERROR, "Failed to start cache clean script");
        else logger.msg(Arc::ERROR, "Cache cleaning script failed");
      }
      for(unsigned int t=CACHE_CLEAN_PERIOD;t;) t=sleep(t);
    }
    if(!have_caches) break;
  }
  return NULL;
}

class sleep_st {
 public:
  Arc::SimpleCondition* sleep_cond;
  CommFIFO* timeout;
  bool to_exit;
  sleep_st(void):sleep_cond(NULL),timeout(NULL),to_exit(false) {
  };
  ~sleep_st(void) {
    to_exit = true;
    while(to_exit) sleep(1);
  };
};

static void* wakeup_func(void* arg) {
  sleep_st* s = (sleep_st*)arg;
  for(;;) {
    if(s->to_exit) break;
    s->timeout->wait();
    if(s->to_exit) break;
    s->sleep_cond->signal();
    if(s->to_exit) break;
  };
  s->to_exit = false;
  return NULL;
}

static void kick_func(void* arg) {
  sleep_st* s = (sleep_st*)arg;
  s->sleep_cond->signal();
}

typedef struct {
  int argc;
  char** argv;
} args_st;

void GridManager::grid_manager(void* arg) {
  //const char* config_filename = (const char*)arg;
  GridManager* gm = (GridManager*)arg;
  GMEnvironment& env = *(gm->Environment());
  if(!arg) return;
  unsigned int clean_first_level=0;
  // int n;
  // int argc = ((args_st*)arg)->argc;
  // char** argv = ((args_st*)arg)->argv;
  setpgid(0,0);
  opterr=0;
  //if(config_filename) nordugrid_config_loc(config_filename);

  logger.msg(Arc::INFO,"Starting grid-manager thread");
  //Daemon daemon;
  // Only supported option now is -c
  /*
  while((n=daemon.getopt(argc,argv,"hvC:c:")) != -1) {
    switch(n) {
      case ':': { logger.msg(Arc::ERROR,"Missing argument"); return; };
      case '?': { logger.msg(Arc::ERROR,"Unrecognized option: %s",(char)optopt); return; };
      case '.': { return; };
      case 'h': {
        std::cout<<"grid-manager [-C clean_level] [-v] [-h] [-c configuration_file] "<<daemon.short_help()<<std::endl;
         return;
      };
      case 'v': {
        std::cout<<"grid-manager: version "<<VERSION<<std::endl;
        return;
      };
      case 'C': {
        if(sscanf(optarg,"%u",&clean_first_level) != 1) {
          logger.msg(Arc::ERROR,"Wrong clean level");
          return;
        };
      }; break;
      case 'c': {
        nordugrid_config_loc=optarg;
      }; break;
      default: { logger.msg(Arc::ERROR,"Option processing error"); return; };
    };
  };
  */

  //JobUsers users(*env);
  JobUsers& users = *(gm->Users());
  std::string my_username("");
  uid_t my_uid=getuid();
  JobUser *my_user = NULL;
  //if(!read_env_vars()) {
  //  logger.msg(Arc::FATAL,"Can't initialize runtime environment - EXITING"); return;
  //};
  
  /* recognize itself */
  {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwuid_r(my_uid,&pw_,buf,BUFSIZ,&pw);
    if(pw != NULL) { my_username=pw->pw_name; };
  };
  if(my_username.length() == 0) {
    logger.msg(Arc::FATAL,"Can't recognize own username - EXITING"); return;
  };
  my_user = new JobUser(env,my_username);
  if(!configure_serviced_users(users,my_uid,my_username,*my_user/*,&daemon*/)) {
    logger.msg(Arc::INFO,"Used configuration file %s",env.nordugrid_config_loc());
    logger.msg(Arc::FATAL,"Error processing configuration - EXITING"); return;
  };
  if(users.size() == 0) {
    logger.msg(Arc::FATAL,"No suitable users found in configuration - EXITING"); return;
  };

  //daemon.logfile(DEFAULT_LOG_FILE);
  //daemon.pidfile(DEFAULT_PID_FILE);
  //if(daemon.daemon() != 0) {
  //  perror("Error - daemonization failed");
  //  exit(1);
  //}; 
  logger.msg(Arc::INFO,"Used configuration file %s",env.nordugrid_config_loc());
  print_serviced_users(users);

  //unsigned int wakeup_period = JobsList::WakeupPeriod();
  CommFIFO wakeup_interface;
  pthread_t wakeup_thread;
  pthread_t cache_thread;
  time_t hard_job_time; 
  sleep_st wakeup_h;
  wakeup_h.sleep_cond=gm->sleep_cond_;
  wakeup_h.timeout=&wakeup_interface;
  for(JobUsers::iterator i = users.begin();i!=users.end();++i) {
    wakeup_interface.add(*i);
  };
  wakeup_interface.timeout(env.jobs_cfg().WakeupPeriod());

  // Prepare signal handler(s). Must be done after fork/daemon and preferably
  // before any new thread is started. 
  //# RunParallel run(&sleep_cond);
  //# if(!run.is_initialized()) {
  //#   logger.msg(Arc::ERROR,"Error - initialization of signal environment failed");
  //#   goto exit;
  //# };

  // I hope nothing till now used Globus

  // It looks like Globus screws signal setup somehow
  //# run.reinit(false);

  /* start timer thread - wake up every 2 minutes */
  if(pthread_create(&wakeup_thread,NULL,&wakeup_func,&wakeup_h) != 0) {
    logger.msg(Arc::ERROR,"Failed to start new thread"); return;
  };
  RunParallel::kicker(&kick_func,&wakeup_h);
  if(clean_first_level) {
    bool clean_finished = false;
    bool clean_active = false;
    bool clean_junk = false;
    if(clean_first_level >= 1) {
      clean_finished=true;
      if(clean_first_level >= 2) {
        clean_active=true;
        if(clean_first_level >= 3) {
          clean_junk=true;
        };
      };
    };
    for(;;) { 
      bool cleaned_all=true;
      for(JobUsers::iterator user = users.begin();user != users.end();++user) {
        size_t njobs = user->get_jobs()->size();
        user->get_jobs()->ScanNewJobs();
        if(user->get_jobs()->size() == njobs) break;
        cleaned_all=false;
        if(!(user->get_jobs()->DestroyJobs(clean_finished,clean_active)))  {
          logger.msg(Arc::WARNING,"Not all jobs are cleaned yet");
          sleep(10); 
          logger.msg(Arc::WARNING,"Trying again");
        };
        kill(getpid(),SIGCHLD);  /* make sure no child is missed */
      };
      if(cleaned_all) {
        if(clean_junk && clean_active && clean_finished) {  
          /* at the moment cleaning junk means cleaning all the files in 
             session and control directories */
          for(JobUsers::iterator user=users.begin();user!=users.end();++user) {
            std::list<FileData> flist;
            for(std::vector<std::string>::const_iterator i = user->SessionRoots().begin(); i != user->SessionRoots().end(); i++) {
              logger.msg(Arc::INFO,"Cleaning all files in directory %s", *i);
              delete_all_files(*i,flist,true);
            }
            logger.msg(Arc::INFO,"Cleaning all files in directory %s", user->ControlDir());
            delete_all_files(user->ControlDir(),flist,true);
          };
        };
        break;
      };
    };
    logger.msg(Arc::INFO,"Jobs cleaned");
  };
  // check if cleaning is enabled for any user, if so activate cleaning thread
  for (JobUsers::const_iterator cacheuser = users.begin(); cacheuser != users.end(); ++cacheuser) {
    if (!cacheuser->CacheParams().getCacheDirs().empty() && cacheuser->CacheParams().cleanCache()) {
      if(pthread_create(&cache_thread,NULL,&cache_func,(void*)(&users))!=0) {
        logger.msg(Arc::INFO,"Failed to start new thread: cache won't be cleaned");
      }
      break;
    }
  }
  /* create control and session directories */
  logger.msg(Arc::INFO,"Preparing directories");
  for(JobUsers::iterator user = users.begin();user != users.end();++user) {
    user->CreateDirectories();
    user->get_jobs()->RestartJobs();
  };
  hard_job_time = time(NULL) + HARD_JOB_PERIOD;
  if (env.jobs_cfg().GetNewDataStaging()) {
    logger.msg(Arc::INFO, "Starting data staging threads");
    DTRGenerator* dtr_generator = new DTRGenerator(users, &kick_func, &wakeup_h);
    if (!(*dtr_generator)) {
      logger.msg(Arc::ERROR, "Failed to start data staging threads, exiting Grid Manager thread");
      return;
    }
    gm->dtr_generator_ = dtr_generator;
    for(JobUsers::iterator user = users.begin();user != users.end();++user) {
      user->get_jobs()->SetDataGenerator(dtr_generator);
    }
  }
  bool scan_old = false;
  std::string heartbeat_file("gm-heartbeat");
  /* main loop - forever */
  logger.msg(Arc::INFO,"Starting jobs' monitoring");
  for(;;) {
    if(gm->tostop_) break;
    users.run_helpers();
    env.job_log().RunReporter(users);
    my_user->run_helpers();
    bool hard_job = ((int)(time(NULL) - hard_job_time)) > 0;
    for(JobUsers::iterator user = users.begin();user != users.end();++user) {
      // touch heartbeat file
      std::string gm_heartbeat(std::string(user->ControlDir() + "/" + heartbeat_file));
      int r = ::open(gm_heartbeat.c_str(), O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
      if (r < 0)
        logger.msg(Arc::WARNING, "Failed to open heartbeat file %s", gm_heartbeat);
      else
        close(r);
      /* check for new marks and activate related jobs */
      user->get_jobs()->ScanNewMarks();
      /* look for new jobs */
      user->get_jobs()->ScanNewJobs();
      /* slowly scan throug old jobs for deleting them in time */
      if(hard_job || scan_old) {
        int max,max_running,max_per_dn,max_total;
        env.jobs_cfg().GetMaxJobs(max,max_running,max_per_dn,max_total);
        scan_old = user->get_jobs()->ScanOldJobs(env.jobs_cfg().WakeupPeriod()/2,max);
      };
      /* process known jobs */
      user->get_jobs()->ActJobs();
    };
    if(hard_job) hard_job_time = time(NULL) + HARD_JOB_PERIOD;
    gm->sleep_cond_->wait();
//#    if(run.was_hup()) {
//#      logger.msg(Arc::INFO,"SIGHUP detected");
//#//      if(!configure_serviced_users(users,my_uid,my_username,*my_user)) {
//#//        std::cout<<"Error processing configuration"<<std::endl; goto exit;
//#//      };
//#    }
//#    else {
      logger.msg(Arc::DEBUG,"Waking up");
//#    };
  };
  // Waiting for children to finish
  logger.msg(Arc::INFO,"Stopping jobs processing thread");
  for(JobUsers::iterator user = users.begin();user != users.end();++user) {
    user->PrepareToDestroy();
    user->get_jobs()->PrepareToDestroy();
  };
  logger.msg(Arc::INFO,"Destroying jobs and waiting for underlying processes to finish");
  for(JobUsers::iterator user = users.begin();user != users.end();++user) {
    delete user->get_jobs();
  };
  gm->active_ = false;
  return;
}

/*
GridManager::GridManager(Arc::XMLNode argv):active_(false) {
  args_st* args = new args_st;
  if(!args) return;
  args->argv=(char**)malloc(sizeof(char*)*(argv.Size()+1));
  args->argc=0;
  args->argv[args->argc]=strdup("grid-manager");
  logger.msg(Arc::VERBOSE, "ARG: %s", args->argv[args->argc]);
  for(;;) {
    Arc::XMLNode arg = argv["arg"][args->argc];
    ++(args->argc);
    if(!arg) break;
    args->argv[args->argc]=strdup(((std::string)arg).c_str());
    logger.msg(Arc::VERBOSE, "ARG: %s", args->argv[args->argc]);
  };
  active_=Arc::CreateThreadFunction(&grid_manager,args);
  if(!active_) delete args;
}
*/

GridManager::GridManager(GMEnvironment& env/*const char* config_filename*/):active_(false),tostop_(false) {
  env_ = &env;
  users_ = new JobUsers(env);
  dtr_generator_ = NULL;
  sleep_cond_ = new Arc::SimpleCondition;
  void* arg = (void*)this; // config_filename?strdup(config_filename):NULL;
  active_=Arc::CreateThreadFunction(&grid_manager,arg);
  //if(!active_) if(arg) free(arg);
}

GridManager::~GridManager(void) {
  logger.msg(Arc::INFO, "Shutting down job processing");
  if(active_) {
    if (dtr_generator_) {
      logger.msg(Arc::INFO, "Shutting down data staging threads");
      delete dtr_generator_;
    }
    // Stop GM thread
    tostop_ = true;
    while(active_) {
      sleep_cond_->signal();
      sleep(1); // TODO: use condition
    }
  }
  delete sleep_cond_;
}

} // namespace ARex

