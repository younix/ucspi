# this Makefile creates files which are needed for tests

KEYLEN=1024

test: tcps tcpc tlss tlsc server.crt client.crt ca.crt
	./test.sh

# create server key ############################################################
client.key:
	openssl genrsa -out $@ ${KEYLEN}
# create server certificate request
client.csr: client.key client.cf
	openssl req -new -key client.key -config client.cf -out $@
# create ca-signed server certificate
client.crt: client.csr ca.crt
	openssl x509 -req -in client.csr -out $@ \
	    -CAcreateserial -CAkey ca.key -CA ca.crt

# create server key ############################################################
server.key:
	openssl genrsa -out $@ ${KEYLEN}
# create server certificate request
server.csr: server.key server.cf
	openssl req -new -key server.key -config server.cf -out $@
# create ca-signed server certificate
server.crt: server.csr ca.crt
	openssl x509 -req -in server.csr -out $@ \
	    -CAcreateserial -CAkey ca.key -CA ca.crt

# create certificate authority #################################################
ca.key:
	openssl genrsa -out $@ ${KEYLEN}
ca.crt: ca.key ca.cf
	openssl req -new -x509 -key ca.key -config ca.cf -out $@
