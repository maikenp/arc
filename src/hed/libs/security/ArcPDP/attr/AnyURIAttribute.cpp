#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include "AnyURIAttribute.h"

namespace ArcSec {

std::string AnyURIAttribute::identifier = "anyURI";

bool AnyURIAttribute::equal(AttributeValue* o){
  AnyURIAttribute *other;
  try{
    other = dynamic_cast<AnyURIAttribute*>(o);
  } catch(std::exception&) { };
  if(other==NULL){
    //std::cerr<<"not AnyURIAttribute"<<std::endl;
    return false;
  }
  if(id != other->id) return false;
  if((value.compare(other->getValue()))==0)  //Now, deal with it the same as StringAttribute.  
    return true;
  else 
    return false;
}

} //namespace ArcSec
