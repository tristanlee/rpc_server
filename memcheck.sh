#!/bin/sh
export VALGRIND_LIB=/usr/lib/valgrind;

#check if vagrind exist
match=$(valgrind --version|grep -c "valgrind");
if [ $match -ge 1 ];then
    echo "valgrind exist";
else 
    echo "valgrind not found";
    exit 0;
fi

if [[ -z $1 ]];then
    echo "error usage";
	exit 0;
fi

logfile=memcheck.log;
if [[ -n $2 ]];then
	logfile=$2;
fi

valgrind --tool=memcheck --leak-check=full --log-file=$logfile $1;

