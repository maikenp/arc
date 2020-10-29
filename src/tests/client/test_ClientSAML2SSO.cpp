#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <signal.h>
#include <stdexcept>

#include <arc/GUID.h>
#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/MCC.h>
#include <arc/communication/ClientInterface.h>
#include <arc/communication/ClientSAML2SSO.h>

int main(void) {
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  Arc::Logger logger(Arc::Logger::rootLogger, "Test");
  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::rootLogger.addDestination(logcerr);

  std::string url_str("https://127.0.0.1:60000/echo");
  Arc::URL url(url_str);

  Arc::MCCConfig mcc_cfg;
  mcc_cfg.AddPrivateKey("../echo/testkey-nopass.pem");
  mcc_cfg.AddCertificate("../echo/testcert.pem");
  mcc_cfg.AddCAFile("../echo/testcacert.pem");
  mcc_cfg.AddCADir("../echo/certificates");

  Arc::NS echo_ns; echo_ns["echo"]="http://www.nordugrid.org/schemas/echo";

  std::string idp_name = "https://idp.testshib.org/idp/shibboleth";
  std::string username = "myself";
  std::string password = "myself";


  /******** Test to service with SAML2SSO **********/

  //Create a HTTP client
  logger.msg(Arc::INFO, "Creating a http client");
  Arc::ClientHTTPwithSAML2SSO *client_http;
  client_http = new Arc::ClientHTTPwithSAML2SSO(mcc_cfg,url);
  logger.msg(Arc::INFO, "Creating and sending request");
  Arc::PayloadRaw req_http;
  //req_http.Insert();
  Arc::PayloadRawInterface* resp_http = NULL;
  Arc::HTTPClientInfo info;
  if(client_http) {
    Arc::MCC_Status status = client_http->process("GET", "echo", &req_http,&info,&resp_http, idp_name, username, password);
    if(!status) {
      logger.msg(Arc::ERROR, "HTTP with SAML2SSO invocation failed");
      throw std::runtime_error("HTTP with SAML2SSO invocation failed");
    }
    if(resp_http == NULL) {
      logger.msg(Arc::ERROR,"There was no HTTP response");
      throw std::runtime_error("There was no HTTP response");
    }
  }
  if(resp_http) delete resp_http;
  if(client_http) delete client_http;


  //Create a SOAP client
  logger.msg(Arc::INFO, "Creating a soap client");
  Arc::ClientSOAPwithSAML2SSO *client_soap;
  client_soap = new Arc::ClientSOAPwithSAML2SSO(mcc_cfg,url);
  logger.msg(Arc::INFO, "Creating and sending request");
  Arc::PayloadSOAP req_soap(echo_ns);
  req_soap.NewChild("echo").NewChild("say")="HELLO";
  Arc::PayloadSOAP* resp_soap = NULL;
  Arc::MCC_Status status = client_soap->process(&req_soap,&resp_soap, idp_name, username, password);
  if(!status) {
    logger.msg(Arc::ERROR, "SOAP with SAML2SSO invocation failed");
    throw std::runtime_error("SOAP with SAML2SSO invocation failed");
  }
  if(resp_soap == NULL) {
    logger.msg(Arc::ERROR,"There was no SOAP response");
    throw std::runtime_error("There was no SOAP response");
  }
  std::string xml_soap;
  resp_soap->GetXML(xml_soap);
  std::cout << "XML: "<< xml_soap << std::endl;
  std::cout << "Response: " << (std::string)((*resp_soap)["echoResponse"]["hear"]) << std::endl;
  if(resp_soap) delete resp_soap;
  if(client_soap) delete client_soap;

  return 0;
}
