#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <arc/Logger.h>
#include "VOMSUtil.h"
#include "Credential.h"


int main(void) {
  Arc::LogStream cdest(std::cerr);
  Arc::Logger::getRootLogger().addDestination(cdest);
  Arc::Logger::getRootLogger().setThreshold(Arc::VERBOSE);

  std::string cert("../../../tests/echo/testcert.pem"); 
  std::string key("../../../tests/echo/testkey-nopass.pem");
  std::string cafile("../../../tests/echo/testcacert.pem"); 
  std::string cadir("../../../tests/echo/certificates");

  int keybits = 2048;
  int proxydepth = 10;

  Arc::Time t;

  /**Generate certificate request on one side, 
  *and sign the certificate request on the other side.*/
  /**1.Use BIO as parameters */

  //Request side
  BIO* req;
  req = BIO_new(BIO_s_mem());
  //Arc::Credential request(t, Arc::Period(24*3600), keybits,  "rfc", "independent");
  Arc::Credential request(t,0,keybits);
  request.GenerateRequest(req);

  //Signing side
  BIO* out; 
  out = BIO_new(BIO_s_mem());
  Arc::Credential proxy;

  Arc::Credential signer(cert, key, cadir, cafile); 
  std::string dn_name = signer.GetDN();
  std::cout<<"DN:--"<<dn_name<<std::endl;

  proxy.InquireRequest(req);
  proxy.SetProxyPolicy("rfc","independent","",-1);
  proxy.SetLifeTime(Arc::Period(24*3600));
  signer.SignRequest(&proxy, out);

  BIO_free_all(req);
  BIO_free_all(out);


  /**2.Use string as parameters */
  //Request side
  std::string req_string;
  std::string out_string;
  Arc::Credential request1(t,0, keybits);
  request1.GenerateRequest(req_string);
  std::cout<<"Certificate request: "<<req_string<<std::endl;

  //Signing side
  Arc::Credential proxy1;
  proxy1.InquireRequest(req_string);
  proxy1.SetProxyPolicy("rfc","independent","",-1);
  proxy1.SetLifeTime(Arc::Period(12*3600));
  signer.SignRequest(&proxy1, out_string);
  
  std::string signing_cert1;
  signer.OutputCertificate(signing_cert1);

  //Back to request side, compose the signed proxy certificate, local private key, 
  //and signing certificate into one file.
  std::string private_key1;
  request1.OutputPrivatekey(private_key1);
  out_string.append(private_key1);
  out_string.append(signing_cert1);
  std::cout<<"Final proxy certificate: " <<out_string<<std::endl;


  /**3.Use file location as parameters*/
  //Request side
  std::string req_file("./request.pem");
  std::string out_file("./out.pem");
  //Arc::Credential request2(t, Arc::Period(168*3600), keybits, "rfc", "inheritAll", "policy.txt", proxydepth);
  Arc::Credential request2(t,0, keybits);
  request2.GenerateRequest(req_file.c_str());

  //Signing side
  Arc::Credential proxy2;
  proxy2.InquireRequest(req_file.c_str());
  proxy2.SetProxyPolicy("rfc", "inheritall", "policy.txt", proxydepth);
  proxy2.SetLifeTime(Arc::Period(168*3600));
  signer.SignRequest(&proxy2, out_file.c_str());

  //Back to request side, compose the signed proxy certificate, local private key,
  //and signing certificate into one file.
  std::string private_key2, signing_cert2, signing_cert2_chain;
  request2.OutputPrivatekey(private_key2);
  signer.OutputCertificate(signing_cert2);
  signer.OutputCertificateChain(signing_cert2_chain);
  std::ofstream out_f(out_file.c_str(), std::ofstream::app);
  out_f.write(private_key2.c_str(), private_key2.size());
  out_f.write(signing_cert2.c_str(), signing_cert2.size());
  out_f.write(signing_cert2_chain.c_str(), signing_cert2_chain.size());
  out_f.close();

  return 0;
}
