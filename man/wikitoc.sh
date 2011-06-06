for FILE in $*; do
	NAME=$(echo $FILE | awk -F . '{print $1}')
	SECT=$(echo $FILE | awk -F . '{print $2}')
	HDR=$(grep -A1 '^\.SH NAME$' $FILE | tail -1 | \
		sed 's/^.*\\- //' | sed 's/\.$//')
	echo "|-"
	echo "|| {{Man|$NAME|$SECT}}"
	echo "|| $HDR"
done
