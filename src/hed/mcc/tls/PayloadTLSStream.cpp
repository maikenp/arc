#include <iostream>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "PayloadTLSStream.h"

namespace Arc {
PayloadTLSStream::PayloadTLSStream(SSL* ssl):ssl_(ssl) {
  //initialize something
  
  return;
};

bool PayloadTLSStream::Get(char* buf,int& size) {
  //ssl read
  ssize_t l=size;
  size=0;
  l=SSL_read(ssl_,buf,l);
  if(l <= 0){
     //handle tls error
     return false;
  }
  size=l;
  // std::cerr<<"PayloadTLSStream::Get: *"; for(int n=0;n<l;n++) std::cerr<<buf[n]; std::cerr<<"*"<<std::endl;
  return true;
}

bool PayloadTLSStream::Get(std::string& buf) {
  char tbuf[1024];
  if(ssl_ == NULL) return false;
  int l = sizeof(tbuf);
  bool result = Get(tbuf,l);
  buf.assign(tbuf,l);
  return result;
}

bool PayloadTLSStream::Put(const char* buf,int size) {
  //ssl write
  ssize_t l;
  if(ssl_ == NULL) return false;
  // std::cerr<<"PayloadTLSStream::Put: *"; for(int n=0;n<size;n++) std::cerr<<buf[n]; std::cerr<<"*"<<std::endl;
  for(;size;){
     l=SSL_write(ssl_,buf,size);
     if(l <= 0){
        //handle tls error
        return false;
     }
     buf+=l; size-=l;
  }
  return true;
}
} // namespace Arc
