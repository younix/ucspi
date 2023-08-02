#!/bin/ksh

. ./tap-functions -u

plan_tests 33

# prepare
expect_env() {
	file="$1"
	key="$2"
	expected="$3"
	output="$(grep "^$key=" "$file")"
	test "$key=$expected" = "$output"
	ok $? "Expected $key to be $expected (found $output)"
}

tmpdir=$(mktemp -d tests_XXXXXX)
touch $tmpdir/tcps.log	# prevent ENOENT in until grep loop later

#########################################################################
# plain server to client communication					#
#########################################################################
./tcps -d 127.0.0.1 0 /usr/bin/env 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do printf . && sleep 1; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)
# start client
./tcpc -d 127.0.0.1 $SERVER_PORT ./read6.sh $tmpdir/env.txt 2>$tmpdir/tcpc.log
CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

ok $? "plain connection server -> client"

kill -9 %1

# server side environment
expect_env $tmpdir/env.txt "TCPREMOTEIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPREMOTEHOST" "localhost"
expect_env $tmpdir/env.txt "TCPREMOTEPORT" "$CLIENT_PORT"
expect_env $tmpdir/env.txt "TCPLOCALIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPLOCALHOST" "localhost"
expect_env $tmpdir/env.txt "TCPLOCALPORT" "$SERVER_PORT"
expect_env $tmpdir/env.txt "PROTO" "TCP"

rm "$tmpdir/env.txt"

#########################################################################
# plain client to server communication					#
#########################################################################
./tcps -d 127.0.0.1 0 ./read0.sh "$tmpdir/env.txt" 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do :; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

./tcpc -d 127.0.0.1 $SERVER_PORT ./write.sh 2>$tmpdir/tcpc.log
CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

ok $? "plain connection client -> server"

kill -9 %1

# client side environment
expect_env $tmpdir/env.txt "TCPREMOTEIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPREMOTEHOST" "localhost"
expect_env $tmpdir/env.txt "TCPREMOTEPORT" "$SERVER_PORT"
expect_env $tmpdir/env.txt "TCPLOCALIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPLOCALHOST" "localhost"
expect_env $tmpdir/env.txt "TCPLOCALPORT" "$CLIENT_PORT"
expect_env $tmpdir/env.txt "PROTO" "TCP"

#########################################################################
# cert checks								#
#########################################################################

# These should have been created by 'make test'
test -f ca.crt && test -f server.crt && test -f server.key && test -f client.crt && test -f client.key
ok $? "Certificates and keys for running tests exist."
# TODO: add more tests here
#h=$(openssl x509 -outform der -in server.crt | sha256)
#printf "SHA256:${h}\n"

#########################################################################
# encrypted client to server communication				#
#########################################################################
./tcps -d 127.0.0.1 0					\
	./tlss -f ca.crt -c server.crt -k server.key	\
	./read0.sh "$tmpdir/env.txt" 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do printf . && sleep 1; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

./tcpc -d 127.0.0.1 $SERVER_PORT			\
	./tlsc -f ca.crt -c client.crt -k client.key	\
	./write.sh 2>$tmpdir/tcpc.log

CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

ok $? "tls connection client -> server"

kill -9 %1

# client side environment
expect_env $tmpdir/env.txt "TCPREMOTEIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPREMOTEHOST" "localhost"
expect_env $tmpdir/env.txt "TCPREMOTEPORT" "$SERVER_PORT"
expect_env $tmpdir/env.txt "TCPLOCALIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPLOCALHOST" "localhost"
expect_env $tmpdir/env.txt "TCPLOCALPORT" "$CLIENT_PORT"
expect_env $tmpdir/env.txt "PROTO" "SSL"

rm "$tmpdir/env.txt"

#########################################################################
# encrypted server to client communication				#
#########################################################################
./tcps -d 127.0.0.1 0					\
	./tlss -C -f ca.crt -c server.crt -k server.key	\
	/usr/bin/env 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do :; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

./tcpc -d 127.0.0.1 $SERVER_PORT			\
	./tlsc -f ca.crt -c client.crt -k client.key	\
	./read6.sh "$tmpdir/env.txt" 2>$tmpdir/tcpc.log

CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

ok $? "tls connection server -> client"

kill -9 %1

# server side environment
expect_env $tmpdir/env.txt "TCPREMOTEIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPREMOTEHOST" "localhost"
expect_env $tmpdir/env.txt "TCPREMOTEPORT" "$CLIENT_PORT"
expect_env $tmpdir/env.txt "TCPLOCALIP" "127.0.0.1"
expect_env $tmpdir/env.txt "TCPLOCALHOST" "localhost"
expect_env $tmpdir/env.txt "TCPLOCALPORT" "$SERVER_PORT"
expect_env $tmpdir/env.txt "PROTO" "SSL"

rm "$tmpdir/env.txt"

# clean up
rm -rf $tmpdir

# vim: set spell spelllang=en:
