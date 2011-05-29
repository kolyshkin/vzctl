for M in $MANS vzquota.8; do
	MM=$(echo $M | sed -e 's/^/<b>/' -e 's/.\([58]\)/<\/b>(\1)/')
	REPL="$REPL -e s#$MM#[[Man/$M|$MM]]#"
done

sed -e '1,/<body>/d' -e '/<\/body>/,$d' \
    -e '/<h1/d' -e '/<a href=/d' -e '/<hr>/d' \
    -e '/<a name=/d' -e '/<\/h[123456]>/d' \
    -e 's/^<h2>\(.*$\)/== \1 ==/' \
    -e 's#http://wiki.openvz.org/\([a-zA-Z0-9_:]*\)#[[\1]]#g' \
    $REPL | \
  awk '/=== / {
	if ($0 !~ /=== .*===/) {
		printf "%s ",$0; getline; print; next}
	}
	{print}' | \
  sed 's/.*\(=== [^=]* ===\).*/\1/' | \
  cat -s
