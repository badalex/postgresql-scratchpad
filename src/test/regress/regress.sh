#!/bin/sh
# $Header$
#
if [ -d ./obj ]; then
	cd ./obj
fi

TZ="PST8PDT7,M04.01.00,M10.05.03"; export TZ

#FRONTEND=monitor
FRONTEND="psql -n -e -q"

echo =============== Notes... =================
echo "You must be already running the postmaster"
echo " for the regression tests to succeed."
echo "The time zone might need to be set to PST/PDT"
echo " for the date and time data types to pass the"
echo " regression tests; to do this type"
echo "  setenv TZ $TZ"
echo " before starting the postmaster."

echo =============== destroying old regression database... =================
destroydb regression

echo =============== creating new regression database... =================
createdb regression
if [ $? -ne 0 ]; then
     echo createdb failed
     exit 1
fi

echo =============== running regression queries ... =================
for i in `cat sql/tests`
do
	echo -n "${i} .. "
	$FRONTEND regression < sql/${i}.sql > results/${i}.out 2>&1
	if [ `diff expected/${i}.out results/${i}.out | wc -l` -ne 0 ]
	then
		echo failed
	else
		echo ok
	fi
done
exit

echo =============== running error queries ... =================
$FRONTEND regression < errors.sql
# this will generate error result code

#set this to 1 to avoid clearing the database
debug=0

if test "$debug" -eq 1
then
echo Skipping clearing and deletion of the regression database
else
echo =============== clearing regression database... =================
$FRONTEND regression < destroy.sql
if [ $? -ne 0 ]; then
     echo the destroy script has an error
     exit 1
fi

exit 0
echo =============== destroying regression database... =================
destroydb regression
if [ $? -ne 0 ]; then
     echo destroydb failed
     exit 1
fi

exit 0
fi
