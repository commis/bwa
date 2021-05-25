SYSTEM_FUN="
getopt assert memset calloc free log
atoi atol atof
strlen strtol strtod strcmp
fprintf fgets fopen fclose
isdigit ispunct
"

callgraph -f main_mem -d ./fastmap.c -F "${SYSTEM_FUN}"
