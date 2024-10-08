#include <iostream>
#include <cxxtools/arg.h>
#include <cxxtools/log.h>
#include <cxxtools/remoteprocedure.h>
#include <cxxtools/bin/rpcclient.h>
#include <cxxtools/sslctx.h>

////////////////////////////////////////////////////////////////////////
// main
//
int main(int argc, char* argv[])
{
  try
  {
    log_init(argc, argv);

    cxxtools::Arg<std::string> ip(argc, argv, 'i');
    cxxtools::Arg<unsigned short> port(argc, argv, 'p', 7003);
    cxxtools::Arg<std::string> cert(argc, argv, 'c');
    cxxtools::Arg<std::string> ca(argc, argv, 'C');

    cxxtools::SslCtx sslCtx;

    if (cert.isSet())
        sslCtx.loadCertificateFile(cert);

    if (ca.isSet())
        sslCtx.setVerify(cxxtools::SslCtx::VERIFY_LEVEL::REQUIRE, ca);

    cxxtools::bin::RpcClient client(ip, port, sslCtx);

    cxxtools::RemoteProcedure<std::string, std::string> echo(client, "echo");

    for (int a = 1; a < argc; ++a)
    {
      std::string v = echo(argv[a]);
      std::cout << v << '\n';
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }
}
