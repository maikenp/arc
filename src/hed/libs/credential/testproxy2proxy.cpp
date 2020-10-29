#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <arc/Logger.h>
#include "Credential.h"

#include <openssl/x509.h>

int main(void) {
  Arc::LogStream cdest(std::cerr);
  Arc::Logger::getRootLogger().addDestination(cdest);
  Arc::Logger::getRootLogger().setThreshold(Arc::DEBUG);

  std::string cafile("../../../tests/echo/testcacert.pem");
  std::string cadir("../../../tests/echo/certificates");

  int keybits = 2048;
  int proxydepth = 10;

  Arc::Time t;

  /**Generate a proxy certificate based on existing proxy certificate*/
  //Request side
  std::string req_file1("./request1.pem");
  std::string out_file1("./proxy1.pem");
  //Arc::Credential request1(t, Arc::Period(168*3600), keybits, "rfc", "independent", "", proxydepth);
  Arc::Credential request1(t, Arc::Period(168*3600), keybits);
  request1.GenerateRequest(req_file1.c_str());

  //Signing side
  Arc::Credential proxy1;
  std::string signer_cert1("./out.pem");
  Arc::Credential signer1(signer_cert1, "", cadir, cafile);
  proxy1.InquireRequest(req_file1.c_str());
  proxy1.SetProxyPolicy("rfc","independent","",proxydepth);
  signer1.SignRequest(&proxy1, out_file1.c_str());

  std::string id_name = signer1.GetIdentityName();
  std::string dn_name = signer1.GetDN();
  std::cout<<"Identity name: "<<id_name<<std::endl;
  std::cout<<"DN name: "<<dn_name<<std::endl;

  //Get the proxy information
  std::string policy = signer1.GetProxyPolicy();
  std::cout<<"Policy information: "<<policy<<std::endl;

  //Back to request side, compose the signed proxy certificate, local private key,
  //and signing certificate into one file.
  std::string private_key1, signing_cert1, signing_cert1_chain;
  request1.OutputPrivatekey(private_key1);
  signer1.OutputCertificate(signing_cert1);
  signer1.OutputCertificateChain(signing_cert1_chain);
  std::ofstream out_f1(out_file1.c_str(), std::ofstream::app);
  out_f1.write(private_key1.c_str(), private_key1.size());
  out_f1.write(signing_cert1.c_str(), signing_cert1.size());
  out_f1.write(signing_cert1_chain.c_str(), signing_cert1_chain.size());
  out_f1.close();

  //Generate one more proxy based on the proxy which just has been generated
  std::string req_file2("./request2.pem");
  std::string out_file2("./proxy2.pem");
  //Arc::Credential request2(t, Arc::Period(168*3600), keybits,  "rfc", "independent", "", proxydepth);
  Arc::Credential request2(t, Arc::Period(168*3600), keybits);
  request2.GenerateRequest(req_file2.c_str());

  //Signing side
  Arc::Credential proxy2;
  std::string signer_cert2("./proxy1.pem");
  Arc::Credential signer2(signer_cert2, "", cadir, cafile);
  proxy2.InquireRequest(req_file2.c_str());
  proxy2.SetProxyPolicy("rfc","independent","",proxydepth);
  signer2.SignRequest(&proxy2, out_file2.c_str());

  id_name = signer2.GetIdentityName();
  dn_name = signer2.GetDN();
  std::cout<<"Identity name: "<<id_name<<std::endl;
  std::cout<<"DN name: "<<dn_name<<std::endl;

  //Back to request side, compose the signed proxy certificate, local private key,
  //and signing certificate into one file.
  std::string private_key2, signing_cert2, signing_cert2_chain;
  request2.OutputPrivatekey(private_key2);
  signer2.OutputCertificate(signing_cert2);
  signer2.OutputCertificateChain(signing_cert2_chain);
  std::ofstream out_f2(out_file2.c_str(), std::ofstream::app);
  out_f2.write(private_key2.c_str(), private_key2.size());
  out_f2.write(signing_cert2.c_str(), signing_cert2.size());
  //Here add the up-level signer certificate
  out_f2.write(signing_cert2_chain.c_str(), signing_cert2_chain.size());
  out_f2.close();

  /*****************************************/
  std::string cert_file("./proxy2.pem"), issuer_file("./proxy1.pem");

  BIO *cert_in=NULL;
  BIO *issuer_in=NULL;
  X509 *cert = NULL;
  X509 *issuer = NULL;
  issuer_in=BIO_new_file(issuer_file.c_str(), "r"); 
  PEM_read_bio_X509(issuer_in,&issuer,NULL,NULL); 

  cert_in=BIO_new_file(cert_file.c_str(), "r");
  PEM_read_bio_X509(cert_in,&cert,NULL,NULL);

  EVP_PKEY *pkey=NULL;
  pkey = X509_get_pubkey(issuer);
  int n = X509_verify(cert,pkey);

  BIO *pubkey;
  std::string pubkey_file("pubkey1.pem");
  pubkey = BIO_new_file(pubkey_file.c_str(), "w");
  PEM_write_bio_PUBKEY(pubkey, pkey);

  std::cout<<"Verification result: "<<(n==1?"Succeeded":"Failed")<<std::endl;

}
