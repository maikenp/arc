#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstring>
#include <map>

#include <sstream>

#include <arc/StringConv.h>
#include <arc/Thread.h>
#include <arc/FileUtils.h>


#include "DataStagingMetrics.h"


#include "../conf/GMConfig.h"

namespace ARex {

static Arc::Logger& logger = Arc::Logger::getRootLogger();

DataStagingMetrics::DataStagingMetrics():enabled(false),proc(NULL) {

}

DataStagingMetrics::~DataStagingMetrics() {
}

void DataStagingMetrics::SetEnabled(bool val) {
  enabled = val;
}

void DataStagingMetrics::SetConfig(const char* fname) {
  config_filename = fname;
}
  
void DataStagingMetrics::SetGmetricPath(const char* path) {
  tool_path = path;
}


  void DataStagingMetrics::ReportDataStagingChange(const GMConfig& config) {
    Glib::RecMutex::Lock lock_(lock);
    

    DataStaging::DTRList DtrList;
    std::list<std::string> alljobs = DtrList.all_jobs();

    // The DTRs ready to go into a processing state
    std::map<DataStaging::DTRStatus::DTRStatusType, std::list<DataStaging::DTR_ptr> > DTRQueueStates;
    DtrList.filter_dtrs_by_statuses(DataStaging::DTRStatus::ToProcessStates, DTRQueueStates);

    // The active DTRs currently in processing states
    std::map<DataStaging::DTRStatus::DTRStatusType, std::list<DataStaging::DTR_ptr> > DTRRunningStates;
    DtrList.filter_dtrs_by_statuses(DataStaging::DTRStatus::ProcessingStates, DTRRunningStates);


    for(std::map<DataStaging::DTRStatus::DTRStatusType,std::list<DataStaging::DTR_ptr> >::iterator dtr_states = DTRQueueStates.begin(); dtr_states != DTRQueueStates.end(); dtr_states ++){

      logger.msg(Arc::DEBUG,"Maikenp map dtr states: %s",dtr_states->first);

    }

    Sync();
}

bool DataStagingMetrics::CheckRunMetrics(void) {
  if(!proc) return true;
  if(proc->Running()) return false;
  int run_result = proc->Result();
  if(run_result != 0) {
   logger.msg(Arc::ERROR,": Metrics tool returned error code %i: %s",run_result,proc_stderr);
  };
  delete proc;
  proc = NULL;
  return true;
}

void DataStagingMetrics::Sync(void) {
  if(!enabled) return; // not configured
  Glib::RecMutex::Lock lock_(lock);
  if(!CheckRunMetrics()) return;
  // Run gmetric to report one change at a time
  //since only one process can be started from Sync(), only 1 histogram can be sent at a time, therefore return for each call;
  //Sync is therefore called multiple times until there are not more histograms that have changed

  /*
  if(time_update){
    if(RunMetrics(
		  std::string("AREX-HEARTBEAT_LAST_SEEN"),
		  Arc::tostring(time_delta), "int32", "sec"
		  )) {
      time_update = false;
      return;
    };
  }
  */


}

 
bool DataStagingMetrics::RunMetrics(const std::string name, const std::string& value, const std::string unit_type, const std::string unit) {
  if(proc) return false;
  std::list<std::string> cmd;
  if(tool_path.empty()) {
    logger.msg(Arc::ERROR,"gmetric_bin_path empty in arc.conf (should never happen the default value should be used");
    return false;
  } else {
    cmd.push_back(tool_path);
  };
  if(!config_filename.empty()) {
    cmd.push_back("-c");
    cmd.push_back(config_filename);
  };
  cmd.push_back("-n");
  cmd.push_back(name);
  cmd.push_back("-v");
  cmd.push_back(value);
  cmd.push_back("-t");//unit-type
  cmd.push_back(unit_type);
  cmd.push_back("-u");//unit
  cmd.push_back(unit);
  
  proc = new Arc::Run(cmd);
  proc->AssignStderr(proc_stderr);
  proc->AssignKicker(&RunMetricsKicker, this);
  if(!(proc->Start())) {
    delete proc;
    proc = NULL;
    return false;
  };
  return true;
}

void DataStagingMetrics::SyncAsync(void* arg) {
  DataStagingMetrics& it = *reinterpret_cast<DataStagingMetrics*>(arg);
  if(&it) {
    Glib::RecMutex::Lock lock_(it.lock);
    if(it.proc) {
      // Continue only if no failure in previous call.
      // Otherwise it can cause storm of failed calls.
      if(it.proc->Result() == 0) {
        it.Sync();
      };
    };
  };
}

void DataStagingMetrics::RunMetricsKicker(void* arg) {
  // Currently it is not allowed to start new external process
  // from inside process licker (todo: redesign).
  // So do it asynchronously from another thread.
  Arc::CreateThreadFunction(&SyncAsync, arg);
}

} // namespace ARex
