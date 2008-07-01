#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "PermitOverridesAlg.h"

namespace ArcSec{

std::string PermitOverridesCombiningAlg::algId = "Permit-Overrides";

Result PermitOverridesCombiningAlg::combine(EvaluationCtx* ctx, std::list<Policy*> policies){
  bool atleast_onedeny = false;
  bool atleast_oneerror = false;
  bool potentialpermit = false;

  std::list<Policy*>::iterator it;
  for(it = policies.begin(); it != policies.end(); it++) {
    Policy* policy = *it;
    MatchResult match = policy->match(ctx);
    
    //Evaluate the policy, if one policy evaluation return positive result, then return DECISION_PERMIT
    if(match == MATCH) {
      Result res = policy->eval(ctx);
      if(res == DECISION_PERMIT)
        return DECISION_PERMIT;

      //Never happen currently      
      if (res == DECISION_INDETERMINATE){
        atleast_oneerror = true;
        if((policy->getEffect()).compare("Permit")==0)
          potentialpermit = true;
      }
      else if(res == DECISION_DENY)
        atleast_onedeny = true;
    }

    //Never happen currently
    else if(match == INDETERMINATE)
      atleast_oneerror = true;

    else if(match == NO_MATCH) {
      Result res = policy->eval(ctx);
      if(res == DECISION_DENY)
        atleast_onedeny = true;
    }
  }
  
  if(potentialpermit) return DECISION_INDETERMINATE;
  if(atleast_onedeny) return DECISION_DENY;
  if(atleast_oneerror) return DECISION_INDETERMINATE;

  return DECISION_NOT_APPLICABLE;
}

} //namespace ArcSec

