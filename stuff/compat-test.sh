#!/bin/bash

failed=no

echo "Going to run unit test as-is"
if ! ./compat-test; then
	echo failed
	failed=yes
fi

echo "Going to run unit test via valgrind"
if ! valgrind --error-exitcode=1  --leak-check=full ./compat-test; then
	echo failed
	failed=yes;
fi

if test x$failed != xno; then
	echo "One or more test failed"
	exit 1
fi

exit 0
