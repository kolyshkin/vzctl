for FILE in $*; do
	NAME=$(echo $FILE | sed 's/\.\([0-9]\)$/\&nbsp;(\1)/')
	HDR=$(grep -A1 '^\.SH NAME$' $FILE | tail -1 | \
		sed 's/^.*\\- //' | sed 's/\.$//')
	echo "|-"
	echo "|| [[/$FILE/|$NAME]]"
	echo "|| $HDR"
done
