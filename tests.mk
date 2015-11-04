# this Makefile creates files which are needed for tests

test: tcps tcpc tlss tlsc server.crt
	./test.sh

# create server key
server.key:
	openssl genrsa -out $@ 2048

# create server certificate request
server.csr: server.key openssl.cf
	openssl req -new -key server.key -config openssl.cf -out $@

# create self-signed server certificate
server.crt: server.csr server.key
	openssl x509 -req -in server.csr -signkey server.key -out $@

# create certificate authority
ca.key:
	openssl genrsa -out $@ 2048

ca.crt: ca.key
	openssl req -new -x509 -key ca.key -out $@
