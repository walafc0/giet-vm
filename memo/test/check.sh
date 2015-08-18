#! /bin/sh

### Debug

#set -x
#set -e

### Colors

RED="\033[0;31m"
GREEN="\033[0;32m"
WHITE="\033[0;37m"

### Code

#TODO

echo -n "Test 1 : "

./memo.x memo.x 2>test1.stderr 1>test1.stdout

if test "1$?" -eq "10"; then
    echo -e $GREEN "OK" $WHITE
else
    echo -e $RED "KO" $WHITE
fi
