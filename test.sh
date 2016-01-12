#!/usr/bin/env bash

. ./tap-functions -u

plan_tests 32

# prepare

file_grep() {
	file=$1
	regex=$2

	grep -q "$regex" $file
	ok $? "environment variable: \"$regex\""
}

tmpdir=$(mktemp -d tests_XXXXXX)

#########################################################################
# plain server to client communication					#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT /usr/bin/env &
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT ./read6.sh $tmpdir/env.txt 				\

ok $? "plain connection server -> client"

kill -9 %1

# server side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=TCP\$"

#########################################################################
# plain client to server communication					#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./read0.sh "$tmpdir/env.txt" &
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT		\
	./write.sh

ok $? "plain connection client -> server"

kill -9 %1

# client side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=TCP\$"

#########################################################################
# encrypted client to server communication				#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./tlss -f ca.crt -c server.crt -k server.key	\
	./read0.sh "$tmpdir/env.txt" &
./tcpc -p $CLIENT_PORT localhost $SERVER_PORT		\
	./tlsc -f ca.crt -c client.crt -k client.key	\
	./write.sh

ok $? "tls connection client -> server"

kill -9 %1

# client side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=SSL\$"

#########################################################################
# encrypted server to client communication				#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./tlss -C -f ca.crt -c server.crt -k server.key	\
	/usr/bin/env &
./tcpc -p $CLIENT_PORT localhost $SERVER_PORT		\
	./tlsc    -f ca.crt -c client.crt -k client.key	\
	./read6.sh "$tmpdir/env.txt"

ok $? "tls connection server -> client"

kill -9 %1

# server side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=SSL\$"

# clean up
rm -rf $tmpdir

# vim: set spell spelllang=en:
