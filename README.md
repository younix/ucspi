# ucspi-tools

The **UNIX Client/Server Program Interface Tool Suite** is bunch of tools to
handle UCSPI connections.

## sockc

*sockc* is an ucpi SOCKS client.  It handles the socks protocol transparently
and establishes further connection through the corresponding SOCKS server.
*sockc* supports SOCKS version 5.

## tlsc

*tlsc* establishes an TLS connection and builds an crypto interface between the
network side and program side of the exec-chain.  It depends on libtls from
LibreSSL.

## tlss

*tlss* accepts server side tls connections.  It also uses libtls for encryption.

## sslc

*sslc* is a legacy version of tlsc which just depends on plain old OpenSSL.  It
just contains rudiment certificate checks.

## httpc

The http client is just a stub for testing.  It needs to be rewritten for
productive use.

## examples

Just open a tcp connection to google.de and make a fetch of the start page.

```shell
tcpclient www.google.de 80 http www.google.de
```

Get the google index page over a local socks proxy:

```shell
tcpclient 127.0.0.1 8080 socks www.google.de 80 ./http.sh www.google.de
```

If you have to use a socks proxy you could always use socks with the following
alias:

```shellscript
alias tcpclient="tcpclient 127.0.0.1 8080 socks"
tcpclient www.google.de 80
```

## TODO:
  * missing, but useful tools
    * http proxy client
    * smtp client
    * socks server
  * sockc
    * user authentication
    * server mode
    * udp
  * tlsc
    * Fingerprint accept
    * Revocation check
    * [OCSP](https://en.wikipedia.org/wiki/Online_Certificate_Status_Protocol)
  * httpc
    * user authentication
    * support for different content encodings
    * keep-alive with queue of paths to download

## references
  * [ucspi](http://cr.yp.to/proto/ucspi.txt)
  * [ucspi-unix](http://untroubled.org/ucspi-unix/)
  * [ucspi-tcp](http://cr.yp.to/ucspi-tcp.html)
  * http server [fnord](http://www.fefe.de/fnord/)
  * [SOCKS Protocol Version 5](http://tools.ietf.org/html/rfc1928)
  * [RFC: Username/Password Authentication for SOCKS V5](https://tools.ietf.org/html/rfc1929)
  * [LibreSSL](http://www.libressl.org/)
