# ucspi-tools

The ``UNIX Client/Server Program Interface Tool Suite`` is bunch of tools to
handle UCSPI connections.

## socks

*socks* is an ucpi SOCKS client.
It handles the socks protocol transparently and establishes further connection
through the corresponding SOCKS server.
*socks* supports SOCKS version 5.

### TODO:
 * authentication
 * server side connections

## tlsc

*tlsc* establishes an TLS connection and builds an crypto interface between
the network side and program side of the exec-chain.
It depends on libtls from LibreSSL.

### TODO:
 * server side TLS handling
 * Fingerprint accept
 * Revocation check
 * OCSP

## examples

Just open a tcp conntection to google.de and make a fetch of
the start page.

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

## references
 * [SOCKS Protocol Version 5](http://tools.ietf.org/html/rfc1928)
 * [RFC: Username/Password Authentication for SOCKS V5](https://tools.ietf.org/html/rfc1929)
 * [LibreSSL](http://www.libressl.org/)
