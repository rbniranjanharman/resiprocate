#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/MasterProfile.hxx"
#include "resiprocate/dum/UserProfile.hxx"
#include "resiprocate/dum/ClientAuthManager.hxx"
#include "resiprocate/dum/ClientRegistration.hxx"
#include "resiprocate/dum/ClientPagerMessage.hxx"
#include "resiprocate/dum/ServerPagerMessage.hxx"
#include "resiprocate/dum/RegistrationHandler.hxx"
#include "resiprocate/dum/PagerMessageHandler.hxx"

#include "resiprocate/PlainContents.hxx"
#include "resiprocate/Pkcs7Contents.hxx"
#include "resiprocate/MultipartSignedContents.hxx"
#include "resiprocate/Mime.hxx"

#include "resiprocate/SecurityAttributes.hxx"
#include "resiprocate/Helper.hxx"

#include "resiprocate/os/Log.hxx"
#include "resiprocate/os/Logger.hxx"

#ifdef WIN32
#include "resiprocate/XWinSecurity.hxx"
#endif

#include "resiprocate/dum/CertMessage.hxx"
#include "resiprocate/dum/RemoteCertStore.hxx"

#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::TEST

class TestSMIMEMessageHandler : public ClientPagerMessageHandler,
                                public ServerPagerMessageHandler,
                                public ClientRegistrationHandler
{
   public:
      TestSMIMEMessageHandler(Security *security) : security(security), _registered(false), _ended(false), _rcvd(false)
      {}

      virtual ~TestSMIMEMessageHandler() 
      {
      }

      void reset()
      {
         _ended = false;
         _rcvd = false;
      }

      bool isRegistered()
      {
         return _registered;
      }

      bool isEnded()
      {
         return _ended;
      }

      bool isRcvd()
      {
         return _rcvd;
      }

      virtual void onMessageArrived(ServerPagerMessageHandle handle,
                                    const SipMessage& message)
      {
         
         InfoLog( << "ServerPagerMessageHandler::onMessageArrived: " );

         SipMessage ok = handle->accept();
         handle->send(ok);

         _rcvd = true;

         InfoLog( << "received: " << message);
         InfoLog( << "received type " << message.header(h_ContentType) );       
         InfoLog( << "Body: " << *message.getContents() << "\n" );         
      }

      virtual void onSuccess(ClientPagerMessageHandle,
                             const SipMessage& status)
      {
         InfoLog( << "ClientMessageHandler::onSuccess\n" );
         _ended = true;
      }

      virtual void onFailure(ClientPagerMessageHandle,
                             const SipMessage& status,
                             std::auto_ptr<Contents> contents)
      {
         InfoLog( << "ClientMessageHandler::onFailure\n" );
         InfoLog( << endl << status << endl);
         _ended = true;
         _rcvd = true;
      }

      virtual void onSuccess(ClientRegistrationHandle,
                             const SipMessage& response)
      {
         InfoLog( << "ClientRegistrationHandler::onSuccess\n" );
         _registered = true;
      }

      virtual void onRemoved(ClientRegistrationHandle)
      {
         InfoLog( << "ClientRegistrationHander::onRemoved\n" );
         exit(-1);
      }
      
      virtual void onFailure(ClientRegistrationHandle,
                             const SipMessage& response)
      {
         InfoLog( << "ClientRegistrationHandler::onFailure\n" );
         exit(-1);
      }

      virtual int onRequestRetry(ClientRegistrationHandle,
                                 int retrySeconds, const SipMessage& response)
      {
         InfoLog( << "ClientRegistrationHandler::onRequestRetry\n" );
         exit(-1);
      }

   protected:
      Security *security;
      bool _registered;
      bool _ended;
      bool _rcvd;
      
};

class MyCertStore : public RemoteCertStore
{
   public:
      MyCertStore() {}
      ~MyCertStore() {}
      void fetch(const Data& target, MessageId::Type type, const MessageId& id, TransactionUser& tu)
      {
         Data empty;
         CertMessage* msg = new CertMessage(id, false, empty);
         tu.post(msg);
      }
};

int main(int argc, char *argv[])
{

   if ( argc < 3 ) {
      cout << "usage: " << argv[0] << " sip:user passwd\n";
      return 0;
   }

   //Log::initialize(Log::Cout, Log::Debug, argv[0]);
   Log::initialize(Log::Cout, Log::Info, argv[0]);

   NameAddr userAor(argv[1]);
   Data passwd(argv[2]);

   InfoLog(<< "user: " << userAor << ", passwd: " << passwd << "\n");

#ifdef WIN32
   Security* security = new XWinSecurity;
#else
   Security* security = new Security;
#endif

   assert(security);

   SipStack clientStack(security);
   DialogUsageManager clientDum(clientStack);
   clientDum.addTransport(UDP, 10000 + rand()&0x7fff, V4);
   //clientDum.addTransport(TCP, 10000 + rand()&0x7fff, V4);
   //clientDum.addTransport(TLS, 10000 + rand()&0x7fff, V4);
   // clientDum.addTransport(UDP, 10000 + rand()&0x7fff, V6);
   // clientDum.addTransport(TCP, 10000 + rand()&0x7fff, V6);
   // clientDum.addTransport(TLS, 10000 + rand()&0x7fff, V6);

   MyCertStore* store = new MyCertStore;
   clientDum.setRemoteCertStore(auto_ptr<RemoteCertStore>(store));

   SharedPtr<MasterProfile> clientProfile(new MasterProfile);   
   auto_ptr<ClientAuthManager> clientAuth(new ClientAuthManager());   
   TestSMIMEMessageHandler clientHandler(security);

   clientDum.setClientAuthManager(clientAuth);
   clientDum.setClientRegistrationHandler(&clientHandler);
   clientDum.setClientPagerMessageHandler(&clientHandler);
   clientDum.setServerPagerMessageHandler(&clientHandler);

   clientProfile->setDefaultFrom(userAor);
   clientProfile->setDigestCredential(userAor.uri().host(), userAor.uri().user(), passwd);
   clientProfile->setDefaultRegistrationTime(70);		
   clientProfile->addSupportedMethod(MESSAGE);
   clientProfile->addSupportedMimeType(MESSAGE, Mime("text", "plain"));
   clientProfile->addSupportedMimeType(MESSAGE, Mime("application", "pkcs7-mime"));
   clientProfile->addSupportedMimeType(MESSAGE, Mime("multipart", "signed"));
   clientDum.setMasterProfile(clientProfile);

   SipMessage& regMessage = clientDum.makeRegistration(userAor);
	
   InfoLog( << regMessage << "Generated register: " << endl << regMessage );
   clientDum.send(regMessage);

   int iteration = 0;
   bool finished = false;
   while (!finished) 
   {

      clientHandler.reset();
      
      bool sent = false;
      while (!clientHandler.isEnded() || !clientHandler.isRcvd() )
      {
         FdSet fdset;
         clientStack.buildFdSet(fdset);

         int err = fdset.selectMilliSeconds(100);
         assert ( err != -1 );
      
         clientStack.process(fdset);
         while (clientDum.process());

         if (!sent && clientHandler.isRegistered())
         {
            try 
            {
               ClientPagerMessageHandle cpmh = clientDum.makePagerMessage(userAor);			               

               switch (iteration) 
               {  
                  case 0:
                  {
                     Contents* contents = new PlainContents(Data("message"));
                     auto_ptr<Contents> content(contents);
                     cpmh.get()->page(content, DialogUsageManager::None);
                     sent = true;
                     break;
                  }
                  case 1:
                  {
                     InfoLog( << "Sending encrypted message" );
                     Contents* contents = new PlainContents(Data("encrypted message"));
                     auto_ptr<Contents> content(contents);
                     cpmh.get()->page(content, DialogUsageManager::Encrypt);
                     sent = true;
                     break;
                  }
                  case 2:
                  {
                     InfoLog( << "Sending signed message" );
                     Contents* contents = new PlainContents(Data("signed message"));
                     auto_ptr<Contents> content(contents);
                     cpmh.get()->page(content, DialogUsageManager::Sign);
                     sent = true;
                     break;
                  }
                  case 3:
                  {
                     InfoLog( << "Sending encrypted and signed  message" );
                     Contents* contents = new PlainContents(Data("encrypted and signed message"));
                     auto_ptr<Contents> content(contents);
                     cpmh.get()->page(content, DialogUsageManager::SignAndEncrypt);
                     sent = true;
                     break;
                  }
                  default:
                  {
                     InfoLog( << "Finished!" );
                     finished = true;
                     break;
                  }
               }
               
               if (finished)
                  break;
            }
               
            catch (...)
            {
               InfoLog( << "failure to send message at iteration " << iteration );
               break;
            }
         }
      }
      ++iteration;
   }

   return 0;
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */
