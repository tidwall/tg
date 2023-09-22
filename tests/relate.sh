#!/bin/bash


set -e
cd $(dirname "${BASH_SOURCE[0]}")
./geosbuild.sh
cc -O3 -Ilibgeos/build/install/include -o relate relate.c \
     libgeos/build/install/lib/libgeos_c.a \
     libgeos/build/install/lib/libgeos.a -lm -pthread -lstdc++
./relate "$1" "$2"


# # ./relate.sh <geom-a> <geom-b>

# a="$1"
# b="$2"

# if [[ "$a" == "" || "$b" == "" ]]; then
#     >&2 echo "bad input"
#     exit 1;
# fi

# ab="`geosop -a "$a" -b "$b" relate`"
# ba="`geosop -a "$b" -b "$a" relate`"

# if [[ "$ab" == *"Exception"* ]]; then
#     >&2 echo "$ab"
#     exit 1;
# fi 

# if [[ "$ba" == *"Exception"* ]]; then
#     >&2 echo "$ba"
#     exit 1;
# fi 

# if [[ "$ab" == "" || "$ba" == "" ]]; then
#     >&2 echo "bad input"
#     exit 1;
# fi

# predicate0() {
#     if [[ "$3" == "coveredby" ]]; then
#         predicate0 "$2" "$1" "covers"
#     else
#         t="`geosop -a "$1" -b "$2" "$3"`"
#         if [[ "$t" == "true" ]]; then
#             echo "T"
#         elif [[ "$t" == "false" ]]; then
#             echo "F"
#         else 
#             >&2 echo "bad op" "$3"
#             exit 1;
#         fi
#     fi
# }

# predicate() {
#     t1="`predicate0 "$1" "$2" "$3"`"
#     t2="`predicate0 "$2" "$1" "$3"`"
#     n="${#3}"
#     n=$((10-n))
#     spaces="`printf "%${n}s" | tr " " " "`"
#     echo "{ \"$3\": $spaces[\"$t1\", \"$t2\"] }"
# }

# echo "{"
# echo "  \"geoms\": ["
# echo "    \"$a\","
# echo "    \"$b\""
# echo "  ],"
# echo "  \"relate\": [ \"$ab\", \"$ba\" ],"
# echo "  \"predicates\": ["
# echo "    $(predicate "$a" "$b" "equals"),"
# echo "    $(predicate "$a" "$b" "intersects"),"
# echo "    $(predicate "$a" "$b" "disjoint"),"
# echo "    $(predicate "$a" "$b" "contains"),"
# echo "    $(predicate "$a" "$b" "within"),"
# echo "    $(predicate "$a" "$b" "covers"),"
# echo "    $(predicate "$a" "$b" "coveredby"),"
# echo "    $(predicate "$a" "$b" "crosses"),"
# echo "    $(predicate "$a" "$b" "overlaps"),"
# echo "    $(predicate "$a" "$b" "touches")"
# echo "  ]"
# echo "}"
