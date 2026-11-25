# Format key/value STATS text into a readable two-column table.
# Accepts lines such as: cache_hits: 10, cache_hits=10, Scheduler policy: SCAN.
BEGIN {
  print "+--------------------------+------------------------------+"
  printf "| %-24s | %-28s |\n", "STATISTIC", "VALUE"
  print "+--------------------------+------------------------------+"
}
{
  line=$0
  sub(/^[[:space:]]+/, "", line)
  sub(/[[:space:]]+$/, "", line)
  if (line == "") next
  if (match(line, /[:=]/)) {
    key=substr(line, 1, RSTART-1)
    value=substr(line, RSTART+1)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    if (key != "") printf "| %-24.24s | %-28.28s |\n", key, value
  }
}
END {
  print "+--------------------------+------------------------------+"
}
