#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/security/ArcPDP/attr/AttributeValue.h>

#include "ArcEvaluationCtx.h"

using namespace Arc;
using namespace ArcSec;

Logger ArcEvaluationCtx::logger(Arc::Logger::rootLogger, "ArcEvaluationCtx");

ArcRequestTuple::ArcRequestTuple() : RequestTuple() {
  NS ns;
  ns["ra"]="http://www.nordugrid.org/schemas/request-arc";
  XMLNode tupledoc(ns,"ra:RequestItem");
  tupledoc.New(tuple);
}

RequestTuple* ArcRequestTuple::duplicate(const RequestTuple* req_tpl) {  
  XMLNode root = tuple;
  //ArcRequestTuple* tpl = dynamic_cast<ArcRequestTuple*>(req_tpl);

  //Reconstruct the XMLNode by using the information inside req_tpl

  //Reconstruct subject 
  Subject::iterator sit;
  Subject req_sub = req_tpl->sub;
  XMLNode subject;
  if(!req_sub.empty())
    subject = root.NewChild("ra:Subject");
  for(sit = req_sub.begin(); sit != req_sub.end(); sit++){
    //Record the object of the Attribute
    RequestAttribute* attr = new RequestAttribute;
    attr->duplicate(*(*sit));
    sub.push_back(attr); 
     
    //Record the xml node of the Attribute
    XMLNode subjectattr = subject.NewChild("ra:Attribute");
    subjectattr = ((*sit)->getAttributeValue())->encode();
    XMLNode subjectattr_attr = subjectattr.NewAttribute("ra:Type");
    subjectattr_attr = ((*sit)->getAttributeValue())->getType();
    subjectattr_attr = subjectattr.NewAttribute("ra:AttributeId");
    subjectattr_attr = ((*sit)->getAttributeValue())->getId();

/*  AttributeValue *attrval;
    attrval = (*sit)->getAttributeValue();
    if(attrval) std::cout<< "Attribute Value:"<< (attrval->encode()).c_str() << std::endl;
*/           
  }
  
  //Reconstruct resource
  Resource::iterator rit;
  Resource req_res = req_tpl->res;
  XMLNode resource;
  if(!req_res.empty())
    resource = root.NewChild("ra:Resource");
  for(rit = req_res.begin(); rit != req_res.end(); rit++){
    RequestAttribute* attr = new RequestAttribute;
    attr->duplicate(*(*rit));
    res.push_back(attr);

    XMLNode resourceattr = resource.NewChild("ra:Attribute");
    resourceattr = ((*rit)->getAttributeValue())->encode();
    XMLNode resourceattr_attr = resourceattr.NewAttribute("ra:Type");
    resourceattr_attr = ((*rit)->getAttributeValue())->getType();
    resourceattr_attr = resourceattr.NewAttribute("ra:AttributeId");
    resourceattr_attr = ((*rit)->getAttributeValue())->getId();
  }

  //Reconstruct action
  Action::iterator ait;
  Action req_act = req_tpl->act;
  XMLNode action;
  if(!req_act.empty())
    action = root.NewChild("ra:Action");
  for(ait = req_act.begin(); ait != req_act.end(); ait++){
    RequestAttribute* attr = new RequestAttribute;
    attr->duplicate(*(*ait));
    act.push_back(attr);

    XMLNode actionattr = action.NewChild("ra:Attribute");
    actionattr = ((*ait)->getAttributeValue())->encode();
    XMLNode actionattr_attr = actionattr.NewAttribute("ra:Type");
    actionattr_attr = ((*ait)->getAttributeValue())->getType();
    actionattr_attr = actionattr.NewAttribute("ra:AttributeId");
    actionattr_attr = ((*ait)->getAttributeValue())->getId();
  }

  //Reconstruct context
  Context::iterator cit;
  Context req_ctx = req_tpl->ctx;
  XMLNode context;
  if(!req_ctx.empty())
    context = root.NewChild("ra:Context");
  for(cit = req_ctx.begin(); cit != req_ctx.end(); cit++){
    RequestAttribute* attr = new RequestAttribute;
    attr->duplicate(*(*cit));
    ctx.push_back(attr);

    XMLNode contextattr = context.NewChild("ra:Attribute");
    contextattr = ((*cit)->getAttributeValue())->encode();
    XMLNode contextattr_attr = contextattr.NewAttribute("ra:Type");
    contextattr_attr = ((*cit)->getAttributeValue())->getType();
    contextattr_attr = contextattr.NewAttribute("ra:AttributeId");
    contextattr_attr = ((*cit)->getAttributeValue())->getId();
  }

  return this;
}

void ArcRequestTuple::erase() {
  while(!(sub.empty())){
    delete sub.back();
    sub.pop_back();
  }

  while(!(res.empty())){
    delete res.back();
    res.pop_back();
  }

  while(!(act.empty())){
    delete act.back();
    act.pop_back();
  }

  while(!(ctx.empty())){
    delete ctx.back();
    ctx.pop_back();
  }
}

ArcRequestTuple::~ArcRequestTuple() {
  while(!(sub.empty())){
    sub.pop_back();
  }

  while(!(res.empty())){
    res.pop_back();
  }

  while(!(act.empty())){
    act.pop_back();
  }

  while(!(ctx.empty())){
    ctx.pop_back();
  }
}

ArcEvaluationCtx::ArcEvaluationCtx(Request* request) : EvaluationCtx(request), req(NULL), reqitem(NULL), evaltuple(NULL) {
  req = request;
}

ArcEvaluationCtx::~ArcEvaluationCtx(){
  //if(req)
  //  delete req;
  while(!(reqtuples.empty())) {
    delete reqtuples.back();
    reqtuples.pop_back();
  } 
}

Request* ArcEvaluationCtx::getRequest () const{
  return req;
}
/*
ArrtibuteValue * EvaluationCtx::getSubjectAttribute(){

}

ArrtibuteValue * EvaluationCtx::getResourceAttribute(){

}

ArrtibuteValue * EvaluationCtx::getActionAttribute(){

}

ArrtibuteValue * EvaluationCtx::getContextAttribute(){
 
}
*/

static void add_tuple(std::list<RequestTuple*>& reqtuples,Subject* subject,Resource* resource,Action* action,Context* context) {
  if(subject || resource || action || context) {
    ArcRequestTuple* reqtuple = new ArcRequestTuple;
    if(subject) reqtuple->sub = *subject;
    if(resource) reqtuple->res = *resource;
    if(action) reqtuple->act = *action;
    if(context) reqtuple->ctx = *context;
    reqtuples.push_back(reqtuple);
  };
}

static void add_contexts(std::list<RequestTuple*>& reqtuples,Subject* subject,Resource* resource,Action* action,CtxList& contexts) {
  if(contexts.empty()) {
    add_tuple(reqtuples,subject,resource,action,NULL);
    return;
  }
  CtxList::iterator cit = contexts.begin();
  for(;cit != contexts.end();++cit) {
    add_tuple(reqtuples,subject,resource,action,&(*cit));
  }
}

static void add_actions(std::list<RequestTuple*>& reqtuples,Subject* subject,Resource* resource,ActList& actions,CtxList& contexts) {
  if(actions.empty()) {
    add_contexts(reqtuples,subject,resource,NULL,contexts);
    return;
  }
  ActList::iterator ait = actions.begin();
  for(;ait != actions.end();++ait) {
    add_contexts(reqtuples,subject,resource,&(*ait),contexts);
  }
}

static void add_resources(std::list<RequestTuple*>& reqtuples,Subject* subject,ResList& resources,ActList& actions,CtxList& contexts) {
  if(resources.empty()) {
    add_actions(reqtuples,subject,NULL,actions,contexts);
    return;
  }
  ResList::iterator rit = resources.begin();
  for(;rit != resources.end();++rit) {
    add_actions(reqtuples,subject,&(*rit),actions,contexts);
  }
}

static void add_subjects(std::list<RequestTuple*>& reqtuples,SubList& subjects,ResList& resources,ActList& actions,CtxList& contexts) {
  if(subjects.empty()) {
    add_resources(reqtuples,NULL,resources,actions,contexts);
    return;
  }
  SubList::iterator sit = subjects.begin();
  for(;sit != subjects.end();++sit) {
    add_resources(reqtuples,&(*sit),resources,actions,contexts);
  }
}


void ArcEvaluationCtx::split(){
  while(!reqtuples.empty()) { 
    delete reqtuples.back();
    reqtuples.pop_back(); 
  }

  ReqItemList reqlist = req->getRequestItems();
 
  logger.msg(VERBOSE,"There are %d RequestItems", reqlist.size());
  
  std::list<RequestItem*>::iterator it;
  for (it = reqlist.begin(); it != reqlist.end(); it++) {
    SubList subjects = (*it)->getSubjects();
    SubList::iterator sit;
    ResList resources = (*it)->getResources();
    ResList::iterator rit;
    ActList actions = (*it)->getActions();
    ActList::iterator ait;
    CtxList contexts = (*it)->getContexts();
    CtxList::iterator cit;
   
    //Scan subjects, resources, actions and contexts inside one RequestItem object
    //to split subjects, resources, actions or contexts into some tuple with one subject, one resource, one action and context
    //See more descrioption in inteface RequestItem.h
    add_subjects(reqtuples,subjects,resources,actions,contexts);
/*
    for(sit = subjects.begin(); sit != subjects.end(); sit++) { //The subject part will never be empty
      if(!resources.empty()) {
        for(rit = resources.begin(); rit != resources.end(); rit++){
          if(!actions.empty()){
            for(ait = actions.begin(); ait != actions.end(); ait++){
              if(!contexts.empty()){
                for(cit = contexts.begin(); cit != contexts.end(); cit++){
                  RequestTuple* reqtuple = new RequestTuple;
                  reqtuple->sub = *sit;
                  reqtuple->res = *rit;
                  reqtuple->act = *ait;
                  reqtuple->ctx = *cit;
                  reqtuples.push_back(reqtuple);
                }
              }
              else {
                RequestTuple* reqtuple = new RequestTuple;
                reqtuple->sub = *sit;
                reqtuple->res = *rit;
                reqtuple->act = *ait;
                reqtuples.push_back(reqtuple);             
              }         
            }
          }
          else {
            if(!contexts.empty()){
              for(cit = contexts.begin(); cit != contexts.end(); cit++){
                RequestTuple* reqtuple = new RequestTuple;
                reqtuple->sub = *sit;
                reqtuple->res = *rit;
                reqtuple->ctx = *cit;
                reqtuples.push_back(reqtuple);
              }
            }
            else {
              RequestTuple* reqtuple = new RequestTuple;
              reqtuple->sub = *sit;
              reqtuple->res = *rit;
              reqtuples.push_back(reqtuple);
            }
          }
        }
      }
      
      else{
        if(!actions.empty()){
          for(ait = actions.begin(); ait != actions.end(); ait++){
            if(!contexts.empty()){
              for(cit = contexts.begin(); cit != contexts.end(); cit++){
                RequestTuple* reqtuple = new RequestTuple;
                reqtuple->sub = *sit;
                reqtuple->act = *ait;
                reqtuple->ctx = *cit;
                reqtuples.push_back(reqtuple);
              }
            }
            else {
              RequestTuple* reqtuple = new RequestTuple;
              reqtuple->sub = *sit;
              reqtuple->act = *ait;
              reqtuples.push_back(reqtuple);
            }
          }
        }
        else {
          if(!contexts.empty()){
            for(cit = contexts.begin(); cit != contexts.end(); cit++){
              RequestTuple* reqtuple = new RequestTuple;
              reqtuple->sub = *sit;
              reqtuple->ctx = *cit;
              reqtuples.push_back(reqtuple);
            }
          }
          else {
            RequestTuple* reqtuple = new RequestTuple;
            reqtuple->sub = *sit;
            reqtuples.push_back(reqtuple);
          }
        }
      }
    }
*/

/*
    for(sit = subjects.begin(); sit != subjects.end(); sit++) {
      for(rit = resources.begin(); rit != resources.end(); rit++){
        for(ait = actions.begin(); ait != actions.end(); ait++){
          for(cit = contexts.begin(); cit != contexts.end(); cit++){
            RequestTuple* reqtuple = new RequestTuple;
            reqtuple->sub = *sit;
            reqtuple->res = *rit;
            reqtuple->act = *ait;
            reqtuple->ctx = *cit;
            reqtuples.push_back(reqtuple);  

            logger.msg(INFO, "Subject size:  %d", (*sit).size());
            Arc::Subject::iterator it;
            for (it = (*sit).begin(); it!= (*sit).end(); it++){
              AttributeValue *attr;
              attr = (*it)->getAttributeValue();
              if(attr!=NULL) 
                logger.msg(INFO, "%s", attr->encode());
            }
          }
        }
      }
    }*/
  }

}
