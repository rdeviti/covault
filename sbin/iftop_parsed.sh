#!/usr/bin/env zsh
#!/bin/env zsh

# usage: ./iftop_parsed [port] [interface]

unbuffer /usr/sbin/iftop -N -n -P -B -b -t -f "port $1" -i $2 | awk '/=>/ { print strftime("%H:%M:%S"), "SENT", $2, $4, $7; fflush() } /<=/ { print strftime("%H:%M:%S"), "RECV", $1, $3, $6 ; fflush() } END { fflush() } '
