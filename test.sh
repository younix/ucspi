#!/bin/sh

tcpclient -4 127.0.0.1 1080 ./socks www.google.de 80 ./http.sh
#tcpclient -4 127.0.0.1 1080 ./socks www.google.de 80 /bin/echo eins zwei drei
