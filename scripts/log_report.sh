#!/bin/bash
# Summarise a request log written by log_request(): counts, errors and latency per command, and per user.
#   ./scripts/log_report.sh <logfile>          readable table
#   ./scripts/log_report.sh <logfile> --csv    csv (for graphs in the report)
LOG=${1:-}
[ -f "$LOG" ] || { echo "usage: $0 <logfile> [--csv]" >&2; exit 1; }
ROWS=$(sed -n 's/.* req: id=[0-9]* user=\([^ ]*\) cmd=\([A-Z]*\) .* -> \(.*\) (\([0-9.]*\) ms)$/\2|\1|\3|\4/p' "$LOG")
[ -n "$ROWS" ] || { echo "no request lines found in $LOG"; exit 0; }

if [ "${2:-}" = "--csv" ]; then
    echo "command,count,errors,avg_ms,max_ms"
    echo "$ROWS" | awk -F'|' '{n[$1]++; t[$1]+=$4; if($4>m[$1])m[$1]=$4; if($3!="OK")e[$1]++}
        END{for(c in n) printf "%s,%d,%d,%.3f,%.3f\n", c, n[c], e[c]+0, t[c]/n[c], m[c]}' | sort
    exit 0
fi

echo "$ROWS" | awk -F'|' '
    {n[$1]++; t[$1]+=$4; if($4>m[$1])m[$1]=$4; if($3!="OK"){e[$1]++; E++}; u[$2]++; N++; T+=$4}
    END{
        printf "%-10s %7s %7s %10s %10s\n", "command", "count", "errors", "avg ms", "max ms"
        for(c in n) printf "%-10s %7d %7d %10.2f %10.2f\n", c, n[c], e[c]+0, t[c]/n[c], m[c] | "sort"
        close("sort")
        printf "%-10s %7d %7d %10.2f\n", "TOTAL", N, E+0, T/N
        printf "\nrequests per user:"
        for(x in u) printf "  %s=%d", x, u[x]
        printf "\n"
    }'
