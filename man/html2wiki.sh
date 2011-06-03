#!/bin/bash
#
# Post-process filter for groff -Thtml for our wiki

# Prepare sed expression to replace known man pages refs with wiki links
for M in $MANS; do
	MM=$(echo $M | sed -e 's/^/<b>/' -e 's/.\([58]\)/<\/b>(\1)/')
	REPL="$REPL -e s#$MM#[[Man/$M|$MM]]#"
done

# The below code tries to:
#  (first sed invocation)
#  - remove everything except contents of <body>...</body>
#  - remove <h1>, <hr> and section links
#  - remove section anchors and closing </hN> tags
#  - change <h2>TEXT to == TEXT ==
#  - use wiki-style links for wiki.openvz.org
#  - apply the above expression for man pages refs
#  (awk)
#  - combine two lines with broken third-level headings (=== ... ===)
#  (second sed invocation)
#  - remove the crap before/after third-level headings
#  (cat -s)
#  - suppress repeated empty lines
sed -e '1,/<body>/d' -e '/<\/body>/,$d' \
    -e '/<h1/d' -e '/<a href=/d' -e '/<hr>/d' \
    -e '/<a name=/d' -e '/<\/h[123456]>/d' \
    -e 's/^<h2>\(.*$\)/== \1 ==/' \
    -e 's#http://wiki.openvz.org/\([a-zA-Z0-9_:]*\)#[[\1]]#g' \
    $REPL | \
  awk '/====* / {
	if ($0 !~ /====* .*====*/) {
		printf "%s ",$0; getline; print; next}
	}
	{print}' | \
  sed 's/.*[^=]\(====* [^=]* ====*\)[^=].*/\1/' | \
  cat -s
