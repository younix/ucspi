# ucspi

UNIX Client/Server Program Interface

## socks

socks is an ucpi socks client.

## sslc

**CAUTION: NO CERTIFICATE CHECKING IS IMPLEMENTED YET!!!**

sslc estableshs an SSL/TLS connection and forwards transparently the traffic
from the following programms.

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
