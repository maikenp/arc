#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <list>
#include <fstream>
#include <stdexcept>

#include <arc/ArcLocation.h>
#include <arc/URL.h>
#include <arc/client/ClientInterface.h>
#include <arc/OptionParser.h>

#include "arex_client.h"

#ifdef WIN32
#include <arc/win32.h>
#endif

static bool html_to_list(const char* html,const Arc::URL base,std::list<Arc::URL>& urls) {
  for(const char* pos = html;;) {
    // Looking for tag
    const char* tag_start = strchr(pos,'<');
    if(!tag_start) break; // No more tags
    // Looking for end of tag
    const char* tag_end = strchr(tag_start+1,'>');
    if(!tag_end) return false; // Broken html?
    // 'A' tag?
    if(strncasecmp(tag_start,"<A ",3) == 0) {
      // Lookig for HREF
      const char* href = strstr(tag_start+3,"href=");
      if(!href) href=strstr(tag_start+3,"HREF=");
      if(href) {
        const char* url_start = href+5;
        const char* url_end = NULL;
        if((*url_start) == '"') {
          ++url_start;
          url_end=strchr(url_start,'"');
          if((!url_end) || (url_end > tag_end)) url_end=NULL;
        } else if((*url_start) == '\'') {
          url_end=strchr(url_start,'\'');
          if((!url_end) || (url_end > tag_end)) url_end=NULL;
        } else {
          url_end=strchr(url_start,' ');
          if((!url_end) || (url_end > tag_end)) url_end=tag_end;
        };
        if(!url_end) return false; // Broken HTML
        std::string url_str(url_start,url_end-url_start);
        Arc::URL url(url_str);
        if(!url) return false; // Bad URL
        if((url.Protocol() == base.Protocol()) &&
           (url.Host() == base.Host()) &&
           (url.Port() == base.Port())) {
          std::string path = url.Path();
          std::string base_path = base.Path();
          if(base_path.length() < path.length()) {
            if(strncmp(base_path.c_str(),path.c_str(),base_path.length()) == 0) {
              urls.push_back(url);
            };
          };
        };
      };
    };
    pos=tag_end+1;
  };
  return true;
}

static bool PayloadRaw_to_string(Arc::PayloadRawInterface& buf,std::string& str,uint64_t& end) {
  end=0;
  for(int n = 0;;++n) {
    const char* content = buf.Buffer(n);
    if(!content) break;
    int size = buf.BufferSize(n);
    if(size > 0) {
      str.append(content,size);
    };
    end=buf.BufferPos(n)+size;
  };
  return true;
}

static bool PayloadRaw_to_stream(Arc::PayloadRawInterface& buf,std::ostream& o,uint64_t& end) {
  end=0;
  for(int n = 0;;++n) {
    const char* content = buf.Buffer(n);
    if(!content) break;
    uint64_t size = buf.BufferSize(n);
    uint64_t pos = buf.BufferPos(n);
    if(size > 0) {
      o.seekp(pos);
      o.write(content,size);
      if(!o) return false;
    };
    end=pos+size;
  };
  return true;
}

// TODO: limit recursion depth
static bool get_file(Arc::ClientHTTP& client,const Arc::URL& url,const std::string& dir) {
  // Read file in chunks. Use first chunk to determine type of file.
  Arc::PayloadRaw req; // Empty request body
  Arc::PayloadRawInterface* resp;
  Arc::HTTPClientInfo info;
  const uint64_t chunk_len = 1024*1024; // Some reasonable size - TODO - make ir configurable
  uint64_t chunk_start = 0;
  uint64_t chunk_end = chunk_len;
  // First chunk
  Arc::MCC_Status r = client.process("GET",url.Path(),chunk_start,chunk_end,&req,&info,&resp);
  if(!r) return false;
  if(!resp) return false;
  if((info.code != 200) && (info.code != 206)) { delete resp; return false; };
  if(strcasecmp(info.type.c_str(),"text/html") == 0) {
    std::string html;
    uint64_t file_size = resp->Size();
    PayloadRaw_to_string(*resp,html,chunk_end);
    delete resp; resp=NULL;
    // Fetch whole html
    for(;chunk_end<file_size;) {
      chunk_start=chunk_end;
      chunk_end=chunk_start+chunk_len;
      r=client.process("GET",url.Path(),chunk_start,chunk_end,&req,&info,&resp);
      if(!r) return false;
      if(!resp) return false;
      if(resp->Size() <= 0) break;
      if((info.code != 200) && (info.code != 206)) { delete resp; return false; };
      PayloadRaw_to_string(*resp,html,chunk_end);
      delete resp; resp=NULL;
    };
    if(resp) delete resp;
    // Make directory
    if(mkdir(dir.c_str(),S_IRWXU) != 0) {
      if(errno != EEXIST) return false;
    };
    // Fetch files
    std::list<Arc::URL> urls;
    if(!html_to_list(html.c_str(),url,urls)) return false;
    for(std::list<Arc::URL>::iterator u = urls.begin();u!=urls.end();++u) {
      if(!get_file(client,*u,dir+(u->Path().substr(url.Path().length())))) return false;
    };
    return true;
  };
  // Start storing file
  uint64_t file_size = resp->Size();
  std::ofstream f(dir.c_str(),std::ios::trunc);
  if(!f) { delete resp; return false; };
  if(!PayloadRaw_to_stream(*resp,f,chunk_end)) { delete resp; return false; };
  delete resp; resp=NULL;
  // Continue fetching file
  for(;chunk_end<file_size;) {
    chunk_start=chunk_end;
    chunk_end=chunk_start+chunk_len;
    r=client.process("GET",url.Path(),chunk_start,chunk_end,&req,&info,&resp);
    if(!r) return false;
    if(!resp) return false;
    if(resp->Size() <= 0) break;
    if((info.code != 200) && (info.code != 206)) { delete resp; return false; };
    if(!PayloadRaw_to_stream(*resp,f,chunk_end)) { delete resp; return false; };
    delete resp; resp=NULL;
  };
  if(resp) delete resp;
  return true;
}

int main(int argc, char* argv[]){

  setlocale(LC_ALL, "");

  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("[service_url] id_file"));

  std::string proxy_path;
  options.AddOption('P', "proxy", istring("path to proxy file"),
		    istring("path"), proxy_path);

  std::string cert_path;
  options.AddOption('C', "certifcate", istring("path to certificate file"),
		    istring("path"), cert_path);

  std::string key_path;
  options.AddOption('K', "key", istring("path to private key file"),
		    istring("path"), key_path);

  std::string ca_dir;
  options.AddOption('A', "cadir", istring("path to CA directory"),
		    istring("directory"), ca_dir);

  std::string config_path;
  options.AddOption('c', "config", istring("path to config file"),
		    istring("path"), config_path);

  std::string local_dir;
  options.AddOption('D', "download", istring("path to download directory"),
		    istring("directory"), local_dir);

  std::string debug;
  options.AddOption('d', "debug",
		    istring("FATAL, ERROR, WARNING, INFO, DEBUG or VERBOSE"),
		    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
		    version);

  std::list<std::string> params = options.Parse(argc, argv);

  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

  if (version) {
    std::cout << Arc::IString("%s version %s", "apget", VERSION) << std::endl;
    return 0;
  }

  try{
    if ((params.size()!=2) && (params.size()!=1))
      throw std::invalid_argument("Wrong number of arguments!");
    std::list<std::string>::reverse_iterator it = params.rbegin();
    std::string jobidarg = *it++;
    std::string jobid;
    std::ifstream jobidfile(jobidarg.c_str());
    if (!jobidfile)
      throw std::invalid_argument("Could not open " + jobidarg);
    std::getline<char>(jobidfile, jobid, 0);
    if (!jobidfile)
      throw std::invalid_argument("Could not read Job ID from " + jobidarg);
    std::string urlstr;
    Arc::XMLNode jobxml(jobid);
    if(!jobxml)
      throw std::invalid_argument("Could not process Job ID from " + jobidarg);
    urlstr=(std::string)(jobxml["ReferenceParameters"]["JobSessionDir"]); // TODO: clever service address extraction
    if(urlstr.empty())
      throw std::invalid_argument("Missing service URL.");
    Arc::URL url(urlstr);
    if(!url)
      throw std::invalid_argument("Can't parse service URL " + urlstr);
    Arc::MCCConfig cfg;
    if(!proxy_path.empty()) cfg.AddProxy(proxy_path);
    if(!key_path.empty()) cfg.AddPrivateKey(key_path);
    if(!cert_path.empty()) cfg.AddCertificate(cert_path);
    if(!ca_dir.empty()) cfg.AddCADir(ca_dir);
    cfg.GetOverlay(config_path);
    Arc::AREXClient ac(url,cfg);
    bool r = get_file(*(ac.SOAP()),url,local_dir);
    if(!r) throw std::invalid_argument("Failed to download files!");
    return EXIT_SUCCESS;
  }
  catch (std::exception& err){
    std::cerr << "ERROR: " << err.what() << std::endl;
    return EXIT_FAILURE;
  }
}
