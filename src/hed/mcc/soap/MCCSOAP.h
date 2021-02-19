#ifndef __ARC_MCCSOAP_H__
#define __ARC_MCCSOAP_H__

#include <arc/message/MCC.h>

namespace ArcMCCSOAP {

using namespace Arc;

  //! A base class for SOAP client and service MCCs.
  /*! This is a base class for SOAP client and service MCCs. It
    provides some common functionality for them, i.e. so far only a
    logger.
   */
  class MCC_SOAP : public MCC {
  public:
    MCC_SOAP(Config *cfg,PluginArgument* parg);
  protected:
    static Logger logger;
  };

/** This MCC parses SOAP message from input payload.
  On input payload with PayloadRawInterface is expected. It's
 converted into PayloadSOAP and passed next MCC. Returned 
 PayloadSOAP is converted into PayloadRaw and returned to calling
 MCC. */
class MCC_SOAP_Service: public MCC_SOAP
{
    public:
        /* Constructor takes configuration of MCC. 
         Currently there are no configuration parameters for this MCC */
        MCC_SOAP_Service(Config *cfg,PluginArgument* parg);
        virtual ~MCC_SOAP_Service(void);
        virtual MCC_Status process(Message&,Message&);
    private:
        bool _continueNonSoap;
};

/* This is client side of SOAP processing MCC.
  It accepts PayloadSOAP kind of payloads as incoming messages and 
 produces same type as outgoing message. Communication to next MCC 
 is done using payloads implementing PayloadRawInterface. */
class MCC_SOAP_Client: public MCC_SOAP
{
    public:
        /* Constructor takes configuration of MCC. 
         Currently there are no configuration parameters for this MCC */
        MCC_SOAP_Client(Config *cfg,PluginArgument* parg);
        virtual ~MCC_SOAP_Client(void);
        virtual MCC_Status process(Message&,Message&);
};

} // namespace ArcMCCSOAP

#endif /* __ARC_MCCSOAP_H__ */
