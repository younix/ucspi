#!/usr/bin/env ksh

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
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT ./read.sh	\
	1> $tmpdir/env.txt 				\

ok $? "plaintext connection"

kill %1

# server side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=TCP\$"

#########################################################################
# plain client to server communication				#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./read.sh "0" "$tmpdir/env.txt" &
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT		\
	./write.sh

ok $? "tls connection client -> server"

kill %1

# client side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=TCP\$"

#########################################################################
# encrypted server to client communication				#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./tlss -c crt.pem -k key.pem			\
	/usr/bin/env &
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT		\
	./tlsc -C					\
	./read.sh 1> $tmpdir/env.txt

ok $? "tls connection server -> client"

kill %1

# server side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=SSL\$"

#########################################################################
# encrypted client to server communication				#
#########################################################################
CLIENT_PORT=$(($RANDOM % 65536 + 1024))
SERVER_PORT=$(($RANDOM % 65536 + 1024))
./tcps 127.0.0.1 $SERVER_PORT				\
	./tlss -c crt.pem -k key.pem			\
	./read.sh "0" "$tmpdir/env.txt" &
./tcpc -p $CLIENT_PORT 127.0.0.1 $SERVER_PORT		\
	./tlsc -C					\
	./write.sh

ok $? "tls connection client -> server"

kill %1

# client side environment
file_grep $tmpdir/env.txt "^TCPREMOTEIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPREMOTEHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPREMOTEPORT=$SERVER_PORT\$"
file_grep $tmpdir/env.txt "^TCPLOCALIP=127.0.0.1\$"
file_grep $tmpdir/env.txt "^TCPLOCALHOST=localhost\$"
file_grep $tmpdir/env.txt "^TCPLOCALPORT=$CLIENT_PORT\$"
file_grep $tmpdir/env.txt "^PROTO=SSL\$"

# clean up
rm -rf $tmpdir

# vim: set spell spelllang=en:
