#include <iostream>
#include <memory>

#include "resiprocate/os/DataStream.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/Helper.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/Helper.hxx"
#include "resiprocate/SdpContents.hxx"
#include "resiprocate/test/TestSupport.hxx"
#include "resiprocate/PlainContents.hxx"
#include "resiprocate/UnknownHeaderType.hxx"
#include "resiprocate/UnknownParameterType.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/ParseBuffer.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::TEST
#define CRLF "\r\n"

int
main(int argc, char** argv)
{
   Log::initialize(Log::Cout, Log::Warning, argv[0]);

   {
      Data txt(
         "To: sip:fluffy@h1.cs.sipit.net\r\n"
         "From: tofu <sip:tofu@ua.ntt.sipit.net>;tag=5494179792598219348\r\n"
         "CSeq: 1 SUBSCRIBE\r\n"
         "Call-ID: 1129541551360711705\r\n"
         "Contact: sip:tofu@ua.ntt.sipit.net:5060\r\n"
         "Event: presence\r\n"
         "Content-Length: 0\r\n"
         "Expires: 3600\r\n"
         "User-Agent: NTT SecureSession User-Agent\r\n"
         "\r\n"
         "--Dfk2rISgfWIirrOJ\r\n"
         "Content-Type: application/pkcs7-signature; name=\"smime.p7s\"\r\n"
         "Content-Transfer-Encoding: binary\r\n"
         "Content-Disposition: attachment; filename=\"smime.p7s\"; handling=required\r\n"
         "0���	*�H�������0���10	+