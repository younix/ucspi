#!/bin/ksh
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
touch $tmpdir/tcps.log	# prevent ENOENT in until grep loop later

#########################################################################
# plain server to client communication					#
#########################################################################
./tcps -d 127.0.0.1 0 /usr/bin/env 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do :; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

# start client
./tcpc -d 127.0.0.1 $SERVER_PORT ./read6.sh $tmpdir/env.txt 2>$tmpdir/tcpc.log
CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

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
./tcps -d 127.0.0.1 0 ./read0.sh "$tmpdir/env.txt" 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do :; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

./tcpc -d 127.0.0.1 $SERVER_PORT ./write.sh 2>$tmpdir/tcpc.log
CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

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
# cert checks								#
#########################################################################

# TODO: add a test here
#h=$(openssl x509 -outform der -in server.crt | sha256)
#printf "SHA256:${h}\n"

#########################################################################
# encrypted client to server communication				#
#########################################################################
./tcps -d 127.0.0.1 0					\
	./tlss -f ca.crt -c server.crt -k server.key	\
	./read0.sh "$tmpdir/env.txt" 2>$tmpdir/tcps.log &

# wait running server
until grep -q '^listen: 127.0.0.1:' $tmpdir/tcps.log; do :; done
SERVER_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcps.log | head -n 1)

./tcpc -d 127.0.0.1 $SERVER_PORT			\
	./tlsc -f ca.crt -c client.crt -k client.key	\
	./write.sh 2>$tmpdir/tcpc.log

CLIENT_PORT=$(sed -ne 's/^listen: 127.0.0.1://p' $tmpdir/tcpc.log | head -n 1)

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
