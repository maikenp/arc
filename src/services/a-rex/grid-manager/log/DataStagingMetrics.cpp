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

  pre_cleaned_update = false;
  transfer_update = false;
  cache_wait_update = false;
  staging_preparing_wait_update = false;

  status_counter.clear();
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


  void DataStagingMetrics::ReportDataStagingChange(DataStaging::DTR_ptr dtr) {
    Glib::RecMutex::Lock lock_(lock);


    //Insert the new DTR into the list if it does not exist, update status if it already exists    
    std::pair<std::map<DataStaging::DTR_ptr,DataStaging::DTRStatus>::iterator,bool> ret;
    ret = dtr_status_map.insert ( std::pair<DataStaging::DTR_ptr,DataStaging::DTRStatus>(dtr,dtr->get_status()) );
    if (ret.second==false) {
      dtr_status_map.find(dtr)->second = dtr->get_status();
    }


    //extract metrics of a selection of states
    std::map<DataStaging::DTR_ptr,DataStaging::DTRStatus>::iterator dtr_it;
    std::map<DataStaging::DTRStatus::DTRStatusType,int>::iterator counter_it;
    for(dtr_it = dtr_status_map.begin(); dtr_it != dtr_status_map.end(); dtr_it ++){
      if(dtr_it->second == DataStaging::DTRStatus::DTRStatusType::PRE_CLEANED){
	if(status_counter.find(DataStaging::DTRStatus::DTRStatusType::PRE_CLEANED)!= status_counter.end()){
	  status_counter[DataStaging::DTRStatus::DTRStatusType::PRE_CLEANED]++;
	  pre_cleaned_update = true;
	}
      }
      else if(dtr_it->second == DataStaging::DTRStatus::DTRStatusType::TRANSFER){
	if(status_counter.find(DataStaging::DTRStatus::DTRStatusType::TRANSFER)!= status_counter.end()){
	  status_counter[DataStaging::DTRStatus::DTRStatusType::TRANSFER]++;
	  transfer_update = true;
	}
      }
      else if(dtr_it->second == DataStaging::DTRStatus::DTRStatusType::CACHE_WAIT){
	if(status_counter.find(DataStaging::DTRStatus::DTRStatusType::CACHE_WAIT)!= status_counter.end()){
	  status_counter[DataStaging::DTRStatus::DTRStatusType::CACHE_WAIT]++;
	  cache_wait_update = true;
	}
      }
      else if(dtr_it->second == DataStaging::DTRStatus::DTRStatusType::STAGING_PREPARING_WAIT){
	if(status_counter.find(DataStaging::DTRStatus::DTRStatusType::STAGING_PREPARING_WAIT)!= status_counter.end()){
	  status_counter[DataStaging::DTRStatus::DTRStatusType::STAGING_PREPARING_WAIT]++;
	  staging_preparing_wait_update = true;
	}
      }
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

  if(pre_cleaned_update){

    if(RunMetrics(
  		  std::string("AREX-ARC_STAGING_PRE_CLEANED"),
  		  Arc::tostring(status_counter[DataStaging::DTRStatus::DTRStatusType::PRE_CLEANED]), "int32", "num"
  		  )) {
      
      pre_cleaned_update = false;
      return;
    }
  }
  if(transfer_update){
    if(RunMetrics(
  		  std::string("AREX-ARC_STAGING_TRANSFER"),
  		  Arc::tostring(status_counter[DataStaging::DTRStatus::DTRStatusType::TRANSFER]), "int32", "num"
  		  )) {

    transfer_update = false;
    return;
    }
  }
  if(cache_wait_update){
    if(RunMetrics(
  		  std::string("AREX-ARC_STAGING_CACHE_WAIT"),
  		  Arc::tostring(status_counter[DataStaging::DTRStatus::DTRStatusType::CACHE_WAIT]), "int32", "num"
  		  )) {
      
      cache_wait_update = false;
      return;
    }
  }
  if(staging_preparing_wait_update){
    
    if(RunMetrics(
  		  std::string("AREX-ARC_STAGING_PREPARING_WAIT"),
  		  Arc::tostring(status_counter[DataStaging::DTRStatus::DTRStatusType::STAGING_PREPARING_WAIT]), "int32", "num"
  		  )) {

    staging_preparing_wait_update = false;
    return;
    }
  }

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
