#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glibmm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <arc/URL.h>
#include <arc/Run.h>
#include <arc/loader/Loader.h>
#include <arc/loader/ServiceLoader.h>
#include <arc/loader/Plexer.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/PayloadStream.h>
#include <arc/ws-addressing/WSA.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>
#include <arc/misc/ClientInterface.h>
#ifdef WIN32
#define NOGDI
#include <objbase.h>
#define sleep(x) Sleep((x)*1000)
#endif

#include "paul.h"

namespace Paul {

// Static initializator
static Arc::Service* get_service(Arc::Config *cfg,Arc::ChainContext*) 
{
    return new PaulService(cfg);
}

bool PaulService::information_collector(Arc::XMLNode &doc)
{
    // refresh dinamic system information
    sysinfo.refresh();
    Arc::XMLNode ad = doc.NewChild("AdminDomain");
    ad.NewChild("ID") = service_id;
    ad.NewChild("Name") = "DesktopPC";
    ad.NewChild("Distriuted") = "no";
    ad.NewChild("Owner") = "someone"; // get user name required
    Arc::XMLNode services = ad.NewChild("Services");
    Arc::XMLNode cs = services.NewChild("ComputingService");
    cs.NewAttribute("type") = "Service";
    cs.NewChild("ID") = service_id;
    cs.NewChild("Type") = "org.nordugrid.paul";
    cs.NewChild("QualityLevel") = "production";
    cs.NewChild("Complexity") = "endpoint=1,share=1,resource=1";
    cs.NewChild("TotalJobs") = Arc::tostring(jobq.getTotalJobs());
    cs.NewChild("RunningJobs") = Arc::tostring(jobq.getRunningJobs());
    cs.NewChild("WaitingJobs") = Arc::tostring(jobq.getWaitingJobs());
    cs.NewChild("StageingJobs") = "0"; // XXX get info from jobqueue
    cs.NewChild("PreLRMSWaitingJobs") = "0"; // XXX get info from jobqueue
    Arc::XMLNode endpoint = cs.NewChild("ComputingEndpoint");
    endpoint.NewAttribute("type") = "Endpoint";
    endpoint.NewChild("ID") = "0"; 
    // endpoint.NewChild("URL") = "none"; // Not exsits
    endpoint.NewChild("Specification") = "iBES";
    endpoint.NewChild("Implementator") = "Nordugrid";
    endpoint.NewChild("ImplementationName") = "paul";
    endpoint.NewChild("ImplementationVersion") = "0.1";
    endpoint.NewChild("QualityLevel") = "production";
    // endpoint.NewChild("WSDL") = "http://"; // not required this side
    endpoint.NewChild("HealthState") = "OK";
    // endpoint.NewChild("ServingState") = "open"; // meaingless
    // endpoint.NewChild("StartTime") = "now";
    endpoint.NewChild("StageingCapabilty") = "yes";
    Arc::XMLNode policy = endpoint.NewChild("Policy");
    policy.NewChild("ID") = "0";
    policy.NewChild("Scheme") = "BASIC";
    policy.NewChild("Rule") = "?";
    policy.NewChild("TrustedCA") = "?"; // XXX get DNs from config ca.pem or dir
#if 0
    Arc::XMLNode activities = endpoint.NewChild("Activities");
    JobList::iterator it;
    for (it = job_list.begin(); it != job_list.end(); it++) {
        Job j = (*it);
        Arc::XMLNode jn = activities.NewChild("ComputingActivity");
        jn.NewAttribute("type") = "Computing";
        jn.NewChild("ID") = j.getID();
        jn.NewChild("Type") = "Computing";
        jn.NewChild("Name") = j.getName(); // from jsdl
        jn.NewChild("State") = (std::string)j.getStatus(); // from metadata
    }
#endif
    Arc::XMLNode r = cs.NewChild("ComputingResource");
    r.NewAttribute("type") = "Resource";
    r.NewChild("ID") = "0";
    r.NewChild("Name") = "DesktopPC";
    r.NewChild("LRMSType") = "Fork/Internal";
    r.NewChild("LRMSVersion") = "0.1";
    r.NewChild("Total") = "1"; // number of exec env
    r.NewChild("Used") = "0"; // comes from running job
    r.NewChild("Homogenity") = "True";
    r.NewChild("SessionDirTotal") = Arc::tostring(SysInfo::diskTotal(job_root));
    r.NewChild("SessionDirFree") = Arc::tostring(SysInfo::diskFree(job_root));
    r.NewChild("SessionDirLifetime") = "10"; // XXX should come from config in sec
    Arc::XMLNode ee = r.NewChild("ExecutionEnvironment");
    ee.NewChild("ID") = "0";
    ee.NewChild("Type") = "RealNode"; // XXX should come form config wheter it is running on VM or not
    ee.NewChild("TotalInstances") = "1";
    ee.NewChild("UsedInstances") = "0"; // if I have multicore machine running 1 job than this is 0 or 1 or what
    ee.NewChild("PhysicalCPUs") = Arc::tostring(sysinfo.getPhysicalCPUs());
    ee.NewChild("LogicalCPUs") = Arc::tostring(sysinfo.getLogicalCPUs());
    ee.NewChild("MainMemorySize") = Arc::tostring(sysinfo.getMainMemorySize());
    ee.NewChild("VirtualMemorySize") = Arc::tostring(sysinfo.getVirtualMemorySize());
    ee.NewChild("OSName") = sysinfo.getOSName();
    ee.NewChild("OSRelease") = sysinfo.getOSRelease();
    ee.NewChild("OSVersion") = sysinfo.getOSVersion();
    ee.NewChild("PlatformType") = sysinfo.getPlatform();
    ee.NewChild("ConnectivityIn") = "False";
    ee.NewChild("ConnectivityOut") = "True";
    r.NewChild("PhysicalCPUs") = Arc::tostring(sysinfo.getPhysicalCPUs());
    r.NewChild("LogicalCPUs") = Arc::tostring(sysinfo.getLogicalCPUs());
    Arc::XMLNode s = cs.NewChild("ComputingShare");
    s.NewAttribute("type") = "Share";
    s.NewChild("ID") = "0";
    s.NewChild("Name") = "AllResources";
    Arc::XMLNode spolicy = s.NewChild("ComputingSharePolicy");
    spolicy.NewAttribute("type") = "SharePolicy";
    Arc::XMLNode sstate = s.NewChild("ComputingShareState");
    sstate.NewAttribute("type") = "ShareState";
    sstate.NewChild("TotalJobs") = Arc::tostring(jobq.getTotalJobs());
    sstate.NewChild("RunningJobs") = Arc::tostring(jobq.getRunningJobs());
    sstate.NewChild("LocalRunningJobs") = Arc::tostring(jobq.getLocalRunningJobs());
    sstate.NewChild("WaitingJobs") = Arc::tostring(jobq.getWaitingJobs());
    sstate.NewChild("State") = "?";
    s.NewChild(policy); // use the same policy
    return true;
}

void PaulService::GetActivities(const std::string &url_str, std::vector<std::string> &ret)
{
    // Collect information about resources
    // and create Glue compatibile Resource description
    Arc::NS glue2_ns;
    glue2_ns["glue2"] = "http://glue2";
    Arc::XMLNode glue2(glue2_ns, "Grid");
    if (information_collector(glue2) == false) {
        logger_.msg(Arc::ERROR, "Cannot collect resource information");
        return;
    }
    // std::string str;
    // glue2.GetXML(str);
    // std::cout << str << std::endl;
    // Create client to url
    Arc::ClientSOAP *client;
    Arc::MCCConfig cfg;
    Arc::URL url(url_str);
    if (url.Protocol() == "https") {
        cfg.AddPrivateKey(pki["PrivateKey"]);
        cfg.AddCertificate(pki["CertificatePath"]);
        cfg.AddCAFile(pki["CACertificatePath"]);
    }
    client = new Arc::ClientSOAP(cfg, url.Host(), url.Port(), url.Protocol() == "https", url.Path());
    // invoke GetActivity SOAP call
    Arc::PayloadSOAP request(ns_);
    request.NewChild("ibes:GetActivities").NewChild(glue2);
    Arc::PayloadSOAP *response;
    Arc::MCC_Status status = client->process(&request, &response);
    if (!status) {
        logger_.msg(Arc::ERROR, "Request failed");
        if (response) {
           std::string str;
           response->GetXML(str);
           logger_.msg(Arc::ERROR, str);
           delete response;
        }
        // delete client;
        return;
    }
    if (!response) {
        logger_.msg(Arc::ERROR, "No response");
        // delete client;
        return;
    }
    
    // Handle soap level error
    Arc::XMLNode fs;
    (*response)["Fault"]["faultstring"].New(fs);
    std::string faultstring = (std::string)fs;
    if (faultstring != "") {
        logger_.msg(Arc::ERROR, faultstring);
        // delete client;
        return;
    }
    // delete client;
    // Create jobs from response
    Arc::XMLNode activities;
    activities = (*response)["ibes:GetActivitiesResponse"]["ibes:Activities"];
    Arc::XMLNode activity;
    for (int i = 0; (activity = activities.Child(i)) != false; i++) {
        Arc::XMLNode id = activity["ActivityIdentifier"];
        if (!id) {
            logger_.msg(Arc::DEBUG, "Missing job identifier");
            continue;
        }
        Arc::WSAEndpointReference epr(id);
        std::string job_id = epr.ReferenceParameters()["sched:JobID"];
        if (job_id.empty()) {
            logger_.msg(Arc::DEBUG, "Cannot find job id");
            continue;
        }
        std::string resource_id = epr.Address();
        if (resource_id.empty()) {
            logger_.msg(Arc::DEBUG, "Cannot find scheduler endpoint");
            continue;
        }
        Arc::XMLNode jsdl = activity["ActivityDocument"]["JobDefinition"];
        JobRequest jr(jsdl);
        Job j(jr);
        j.setStatus(NEW);
        j.setID(job_id);
        j.setResourceID(resource_id); // here the resource is the scheduler endpoint
        ret.push_back(job_id);
        jobq.addJob(j);
        std::string s = sched_status_to_string(j.getStatus());
        logger_.msg(Arc::DEBUG, "Status: %s %d",s,j.getStatus());
        // j.save();
    }
}

typedef struct {
    PaulService *self;
    std::string *job_id;
} ServiceAndJob;

void PaulService::process_job(void *arg)
{
    ServiceAndJob &info = *((ServiceAndJob *)arg);
    PaulService &self = *(info.self);
    Job &j = self.jobq[*(info.job_id)];
    self.logger_.msg(Arc::DEBUG, "Process job: %s", j.getID());
    self.stage_in(j);
    self.run(j);
    self.stage_out(j);
    if (j.getStatus() != KILLED | j.getStatus() != KILLING) {
        j.setStatus(FINISHED);
    }
    // free memory
    delete info.job_id;
    delete &info;
    self.logger_.msg(Arc::DEBUG, "Finished job %s", j.getID());
}

void PaulService::do_request(void)
{
    // XXX pickup scheduler randomly from schdeuler list
    std::string url = (*schedulers.begin());
    logger_.msg(Arc::DEBUG, "Do Request: %s", url);
    std::vector<std::string> job_ids;
    GetActivities(url, job_ids);
    for (int i = 0; i < job_ids.size(); i++) {
        ServiceAndJob *arg = new ServiceAndJob;
        arg->self = this;
        arg->job_id = new std::string(job_ids[i]);
        Arc::CreateThreadFunction(&process_job, arg);
    }
}

// Main request loop
void PaulService::request_loop(void* arg) 
{
    PaulService *self = (PaulService *)arg;
    
    for (;;) {
        self->do_request();
        sleep(self->period);       
    }
}

// Report status of jobs
void PaulService::do_report(void)
{
    logger_.msg(Arc::DEBUG, "Report statues");
    std::map<const std::string, Job *> all = jobq.getAllJobs();
    std::map<const std::string, Job *>::iterator it;
    std::map<std::string, Arc::PayloadSOAP *> requests;

    for (it = all.begin(); it != all.end(); it++) {
        Job *j = it->second;
        std::string sched_url = j->getResourceID();
        Arc::XMLNode report;
        std::map<std::string, Arc::PayloadSOAP *>::iterator r = requests.find(sched_url);
        if (r == requests.end()) {
            Arc::PayloadSOAP *request = new Arc::PayloadSOAP(ns_);
            report = request->NewChild("ibes:ReportActivitiesStatus");
            requests[sched_url] = request;
        } else {
            report = (*r->second)["ibes:ReportActivitiesStatus"];
        }
        
        Arc::XMLNode resp = report.NewChild("ibes:Activity");
        
        // Make response
        Arc::WSAEndpointReference identifier(resp.NewChild("ibes:ActivityIdentifier"));
        identifier.Address(j->getResourceID()); // address of scheduler service
        identifier.ReferenceParameters().NewChild("sched:JobID") = j->getID();

        Arc::XMLNode state = resp.NewChild("ibes:ActivityStatus");
        state.NewAttribute("ibes:state") = sched_status_to_string(j->getStatus());
        std::string s;
        report.GetXML(s);
        logger_.msg(Arc::DEBUG, s);
    }
    
    Arc::MCCConfig cfg;
    Arc::ClientSOAP *client;

    std::map<std::string, Arc::PayloadSOAP *>::iterator i;
    for (i = requests.begin(); i != requests.end(); i++) {
        std::string url_str = i->first;
        Arc::PayloadSOAP *request = i->second;
        Arc::URL url(url_str);
        if (url.Protocol() == "https") {
            cfg.AddPrivateKey(pki["PrivateKey"]);
            cfg.AddCertificate(pki["CertificatePath"]);
            cfg.AddCAFile(pki["CACertificatePath"]);
        }
        client = new Arc::ClientSOAP(cfg, url.Host(), url.Port(), url.Protocol() == "https", url.Path());

        Arc::PayloadSOAP *response;
        Arc::MCC_Status status = client->process(request, &response);
        if (!status) {
            logger_.msg(Arc::ERROR, "Request failed");
            if (response) {
                std::string str;
                response->GetXML(str);
                logger_.msg(Arc::ERROR, str);
                delete response;
            }
            // delete client;
            continue;
        }
        if (!response) {
            logger_.msg(Arc::ERROR, "No response");
            delete response;
            // delete client;
            continue;
        }
    
        // Handle soap level error
        Arc::XMLNode fs;
        (*response)["Fault"]["faultstring"].New(fs);
        std::string faultstring = (std::string)fs;
        if (faultstring != "") {
            logger_.msg(Arc::ERROR, faultstring);
        }
        delete response;
        // delete client;
    }
    // free
    for (i = requests.begin(); i != requests.end(); i++) {
        delete i->second;
    }
}

void PaulService::do_action(void)
{
    logger_.msg(Arc::DEBUG, "Get activity status changes");   
    std::map<const std::string, Job *> all = jobq.getAllJobs();
    std::map<const std::string, Job *>::iterator it;
    std::map<std::string, Arc::PayloadSOAP *> requests;
    // collect schedulers 
    for (it = all.begin(); it != all.end(); it++) {
        Job *j = it->second;
        std::string sched_url = j->getResourceID();
        Arc::XMLNode get;
        std::map<std::string, Arc::PayloadSOAP *>::iterator r = requests.find(sched_url);
        if (r == requests.end()) {
            Arc::PayloadSOAP *request = new Arc::PayloadSOAP(ns_);
            get = request->NewChild("ibes:GetActivitiesStatusChanges");
            requests[sched_url] = request;
        } else {
            get = (*r->second)["ibes:GetActivitiesStatusChanges"];
        }
        // Make response
        Arc::WSAEndpointReference identifier(get.NewChild("ibes:ActivityIdentifier"));
        identifier.Address(j->getResourceID()); // address of scheduler service
        identifier.ReferenceParameters().NewChild("sched:JobID") = j->getID();
    }
    
    Arc::MCCConfig cfg;
    // call get activitiy changes to all scheduler
    std::map<std::string, Arc::PayloadSOAP *>::iterator i;
    for (i = requests.begin(); i != requests.end(); i++) {
        std::string sched_url = i->first;
        Arc::PayloadSOAP *request = i->second;
        Arc::ClientSOAP *client;
        Arc::URL url(sched_url);
        if (url.Protocol() == "https") {
            cfg.AddPrivateKey(pki["PrivateKey"]);
            cfg.AddCertificate(pki["CertificatePath"]);
            cfg.AddCAFile(pki["CACertificatePath"]);
        }
        client = new Arc::ClientSOAP(cfg, url.Host(), url.Port(), url.Protocol() == "https", url.Path());
        Arc::PayloadSOAP *response;
        Arc::MCC_Status status = client->process(request, &response);
        if (!status) {
            logger_.msg(Arc::ERROR, "Request failed");
            if (response) {
                std::string str;
                response->GetXML(str);
                logger_.msg(Arc::ERROR, str);
                delete response;
            }
            // delete client;
            continue;
        }
        if (!response) {
            logger_.msg(Arc::ERROR, "No response");
            delete response;
            // delete client;
            continue;
        }
    
        // Handle soap level error
        Arc::XMLNode fs;
        (*response)["Fault"]["faultstring"].New(fs);
        std::string faultstring = (std::string)fs;
        if (faultstring != "") {
            logger_.msg(Arc::ERROR, faultstring);
        }
        
        std::string s;
        (*response).GetXML(s);
std::cout << s << std::endl;
        // process response
        Arc::XMLNode activities = (*response)["ibes:GetActivitiesStatusChangesResponse"]["Activities"];
        Arc::XMLNode activity;
        for (int i = 0; (activity = activities["Activity"][i]) != false; i++) {
            Arc::XMLNode id = activity["ActivityIdentifier"];
            Arc::WSAEndpointReference epr(id);
            std::string job_id = epr.ReferenceParameters()["sched:JobID"];
            if (job_id.empty()) {
                logger_.msg(Arc::WARNING, "Cannot find job id");
            }
            std::string new_status = (std::string)activity["NewState"];
            Job &j = jobq[job_id];
            j.setStatus(sched_status_from_string(new_status));
            // do actions
            if (j.getStatus() == KILLED) { 
                j.setStatus(FINISHED);
            }
            if (j.getStatus() == KILLING) {
                Arc::Run *run = runq[job_id];
                logger_.msg(Arc::DEBUG, "Killing %s", job_id);
                run->Kill(1);
                j.setStatus(KILLED);
            }
        }
        delete response;
    }
    // free
    for (i = requests.begin(); i != requests.end(); i++) {
        delete i->second;
    }
    // cleanup finished process
}

// Main reported loop
void PaulService::report_and_action_loop(void *arg)
{
    PaulService *self = (PaulService *)arg;
    for (;;) {
        self->do_report();
        self->do_action();
        sleep((int)(self->period*1.1));
    }
}

// Constructor
PaulService::PaulService(Arc::Config *cfg):Service(cfg),logger_(Arc::Logger::rootLogger, "Paul") 
{
    // Define supported namespaces
    ns_["ibes"] = "http://www.nordugrid.org/schemas/ibes";
    ns_["glue2"] = "http://glue2";
    ns_["sched"] = "http://www.nordugrid.org/schemas/sched";
    ns_["wsa"] = "http://www.w3.org/2005/08/addressing";

    service_id = (std::string)((*cfg)["UUID"]);
    std::string sched_endpoint = (std::string)((*cfg)["SchedulerEndpoint"]);
    schedulers.push_back(sched_endpoint);
    period = Arc::stringtoi((std::string)((*cfg)["RequestPeriod"]));
    job_root = (std::string)((*cfg)["JobRoot"]);
    db_path = (std::string)((*cfg)["DataDirectoryPath"]);
    cache_path = (std::string)((*cfg)["CacheDirectoryPath"]);
    timeout = Arc::stringtoi((std::string)((*cfg)["Timeout"]));
    pki["CertificatePath"] = (std::string)((*cfg)["CertificatePath"]);
    pki["PrivateKey"] = (std::string)((*cfg)["PrivateKey"]);  
    pki["CACertificatePath"] = (std::string)((*cfg)["CACertificatePath"]);  
    // Start sched thread
    Arc::CreateThreadFunction(&request_loop, this);
    // Start report and action thread
    Arc::CreateThreadFunction(&report_and_action_loop, this);
}

// Destructor
PaulService::~PaulService(void) 
{
    // NOP
}

}; // namespace Paul

service_descriptors ARC_SERVICE_LOADER = {
    { "paul", 0, &Paul::get_service },
    { NULL, 0, NULL }
};
