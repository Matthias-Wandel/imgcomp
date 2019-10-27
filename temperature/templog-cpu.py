#!/bin/bash
if [ "$QUERY_STRING" ]; then
    if [ "${QUERY_STRING:0:1}" = "c" ]; then
        QUERY_STRING="${QUERY_STRING:1}"
        CAT_DATA=1
    fi
else
    QUERY_STRING=7
fi
tail -`expr $QUERY_STRING \* 96` ../templog.txt > /ramdisk/1week.txt
if [ "$CAT_DATA" ]; then
    echo "Content-Type: text/html"
    echo
    cat /ramdisk/1week.txt
else
    ./templog-gnuplot
fi
rm /ramdisk/1week.txt

