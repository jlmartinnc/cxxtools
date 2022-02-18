/*
 * Copyright (C) 2022 Tommi Maekitalo
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef CXXTOOLS_SSLCTX_H
#define CXXTOOLS_SSLCTX_H

#include <string>

namespace cxxtools
{
class SslCtx
{
public:
    class Impl;

    enum class PROTOCOL_VERSION : unsigned short {
        SSLv2,
        SSLv3,
        TLSv10,
        TLSv11,
        TLSv12,
        TLSv13
    };

    SslCtx()
        : _impl(nullptr)
        { }

    SslCtx(const SslCtx& sslCtx);
    SslCtx& operator= (const SslCtx& sslCtx);
    ~SslCtx();

    bool enabled() const   { return _impl != nullptr; }
    SslCtx&  enable(bool sw = true);
    void disable() { enable(false); }

    SslCtx& loadCertificateFile(const std::string& certFile, const std::string& privateKeyFile = "");

    /** Enables ssl peer certificate check.
     *
     *  Level 0: Disables peer certificate checking, which is the default.
     *
     *  Level 1: In server mode sends a client certificate request.  Verify
     *  peer certificate and fail if not valid.  In server mode accept if
     *  client sends no certificate.
     *
     *  Level 2: As level 1 but in server mode do not accept ssl connection
     *  without client certificate.
     *
     *  A `ca` must be given if `level` > 0. It specifies the certification
     *  authority file or directory.
     *
     *  In any case the delegate `acceptSslCertificate` is called if
     *  connected.  If the delegate returns false, the ssl connection is
     *  denied. A exception of type `cxxtools::SslCertificateNotAccepted`
     *  is thrown.
     *
     *  @deprecated
     *  Use the method cxxtools::net::TcpServer::setSslVerify for server
     *  or setSslVerify without level in tcp clients.
     */
    SslCtx& setVerify(int level, const std::string& ca = std::string());

    /** Enables ssl peer certificate check.
     *
     *  This method is for tcp clients to check the certificate against
     *  a CA. It must not be used in server mode (sockets accepted from
     *  a tcp server class)
     */
    SslCtx& setVerify(const std::string& ca);

    SslCtx& setProtocolVersion(PROTOCOL_VERSION min_version, PROTOCOL_VERSION max_version = PROTOCOL_VERSION::TLSv13);
    SslCtx& setCiphers(const std::string& ciphers);

    /** return standard ctx */
    static SslCtx standard() { return SslCtx().enable(); }

    /** return ctx which is considered to be secure */
    static SslCtx secure();

    /// @internal
    Impl* impl();
    const Impl* impl() const;

private:
    Impl* _impl;
};
}

#endif
