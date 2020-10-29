#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <signal.h>

#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/message/SOAPEnvelope.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/MCC.h>
#include <arc/message/MCCLoader.h>
#include <arc/communication/ClientInterface.h>

static Arc::Logger& logger = Arc::Logger::rootLogger;

int main(void) {

  setlocale(LC_ALL, "");

  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
//  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  //This is an example that shows how the client or the client called 
  //by a service, delegates a proxy to a delegation service.

  //Note the "DelegationServiceEndpoint" should be changed according
  //the actual delegation endpoint.
  Arc::XMLNode sechanlder_nd("\
        <SecHandler name='delegation.handler' id='delegation' event='outgoing'>\
          <Type>x509</Type>\
          <Role>delegator</Role>\
          <!--DelegationServiceEndpoint>https://127.0.0.1:60000/delegation</DelegationServiceEndpoint-->\
          <DelegationServiceEndpoint>https://glueball.uio.no:60000/delegation</DelegationServiceEndpoint>\
          <PeerServiceEndpoint>https://127.0.0.1:60000/echo</PeerServiceEndpoint>\
          <KeyPath>../echo/userkey-nopass.pem</KeyPath>\
          <CertificatePath>../echo/usercert.pem</CertificatePath>\
          <!--ProxyPath>/tmp/5612d050.pem</ProxyPath-->\
          <!--DelegationCredIdentity>/O=KnowARC/OU=UiO/CN=squark.uio.no</DelegationCredIdentity-->\
          <CACertificatePath>../echo/testcacert.pem</CACertificatePath>\
          <CACertificatesDir>../echo/certificates</CACertificatesDir>\
        </SecHandler>");

  /*For the firstly client in the service invocation chain, the credential path
    should be configured for the 'delegator' role delegation handler.
     <KeyPath>../echo/testkey-nopass.pem</KeyPath>\
     <CertificatePath>../echo/testcert.pem</CertificatePath>\
     <!--ProxyPath>/tmp/5612d050.pem</ProxyPath-->\
    Alternatively, For the clients which are called in the intermediate 
    service inside the service invocation chain, the the 'Identity' should 
    be configured for the 'delegator' role delegation handler. The 'Identity' 
    can be parsed from the 'incoming' message context of the service itself 
    by service implementation: 
      std::string identity= msg->Attributes()->get("TLS:IDENTITYDN");
    Afterwards, the service implementation should change the client 
    (the client that this service will call to contact the next intemediate service) 
    configuration to add 'DelegationCredIdentity' like the following.
    <DelegationCredIdentity>/O=KnowARC/OU=UiO/CN=squark.uio.no</DelegationCredIdentity>\

    Filling "DelegationCredIdentity" element is the only code that is needed for 
    the ARC services that need to utilize the delegation functionality (more 
    specifically, to launch a more level of delegation).
  */

  std::string url_str("https://127.0.0.1:60000/echo");
  Arc::URL url(url_str);

  Arc::MCCConfig mcc_cfg;
  mcc_cfg.AddPrivateKey("../echo/userkey-nopass.pem");
  mcc_cfg.AddCertificate("../echo/usercert.pem");
  mcc_cfg.AddCADir("../echo/certificates");
  mcc_cfg.AddCAFile("../echo/testcacert.pem");

  //Create a SOAP client
  logger.msg(Arc::INFO, "Creating a soap client");

  Arc::ClientSOAP *client;
  client = new Arc::ClientSOAP(mcc_cfg,url,60);
  client->AddSecHandler(sechanlder_nd, "arcshc");

  //Create and send echo request
  logger.msg(Arc::INFO, "Creating and sending request");
  Arc::NS echo_ns; echo_ns["echo"]="http://www.nordugrid.org/schemas/echo";
  Arc::PayloadSOAP req(echo_ns);
  req.NewChild("echo").NewChild("say")="HELLO";

  Arc::PayloadSOAP* resp = NULL;

  std::string str;
  req.GetXML(str);
  std::cout<<"request: "<<str<<std::endl;
  Arc::MCC_Status status = client->process(&req,&resp);
  if(!status) {
    logger.msg(Arc::ERROR, "SOAP invocation failed");
  }
  if(resp == NULL) {
    logger.msg(Arc::ERROR,"There was no SOAP response");
  }

  std::string xml;
  resp->GetXML(xml);
  std::cout << "XML: "<< xml << std::endl;
  std::cout << "Response: " << (std::string)((*resp)["echoResponse"]["hear"]) << std::endl;

  if(resp) delete resp;
  if(client) delete client;

  return 0;
}
