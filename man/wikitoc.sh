for FILE in $*; do
	NAME=$(echo $FILE | sed 's/\.[0-9]$//')
	SECT=$(echo $FILE | sed 's/^.*\.\([0-9]\)$/\1/')
	HDR=$(grep -A1 '^\.SH NAME$' $FILE | tail -1 | \
		sed 's/^.*\\- //' | sed 's/\.$//')
	echo "|-"
	echo "|| {{Man|$NAME|$SECT}}"
	echo "|| $HDR"
done
