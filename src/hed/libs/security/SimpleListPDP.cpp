#include <fstream>
#include <sys/types.h>

#include "../../libs/loader/PDPLoader.h"
#include "../../../libs/common/XMLNode.h"
#include "../../../libs/common/Thread.h"
#include "../../../libs/common/ArcConfig.h"

#include "SimpleListPDP.h"

/*static Arc::PDP* get_pdp(Arc::Config *cfg,Arc::ChainContext *ctx) {
    return new Arc::SimpleListPDP(cfg);
}

pdp_descriptor __arc_pdp_modules__[] = {
    { "simplelist.pdp", 0, &get_pdp},
    { NULL, 0, NULL }
};
*/
using namespace Arc;

PDP* SimpleListPDP::get_simplelist_pdp(Config *cfg,ChainContext *ctx) {
    return new SimpleListPDP(cfg);
}

SimpleListPDP::SimpleListPDP(Config* cfg):PDP(cfg){
  location = (std::string)(cfg->Attribute("location"));
  logger.msg(INFO, "Access list location: %s", location.c_str());
}

bool SimpleListPDP::isPermitted(std::string subject){
  std::string line;
  std::ifstream fs(location.c_str());
  
  while (!fs.eof()) {
     getline (fs, line);
     logger.msg(INFO, "policy line: %s", line.c_str());
     logger.msg(INFO, "subject: %s", subject.c_str());
     if(!(line.compare(subject))){
	fs.close();
        return true;
     }
  }
  fs.close();
  logger.msg(ERROR, "UnAuthorized!!!");
  return false;
}
