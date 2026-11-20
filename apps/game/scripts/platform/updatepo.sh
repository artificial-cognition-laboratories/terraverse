#!/bin/sh

# Update/create luanti po files

# an auxiliary function to abort processing with an optional error
# message
abort() {
	test -n "$1" && echo >&2 "$1"
	exit 1
}

# Run from the repository root regardless of the caller's directory.
scriptisin="$( cd "$( dirname "$0" )" && pwd )"

cd "$scriptisin/../.."

test -d assets/po || abort "assets/po/ directory not found"

# Get a list of the languages we have to update/create

cd assets/po || abort "couldn't change directory to assets/po!"

# This assumes that we won't have dirnames with space, which is
# the case for language codes, which are the only subdirs we expect to
# find in po/ anyway. If you put anything else there, you need to suffer
# the consequences of your actions, so we don't do sanity checks
langs=""

for lang in * ; do
	if test ! -d $lang; then
		continue
	fi
	langs="$langs $lang"
done

# go back
cd ..

# First thing first, update the .pot template. We place it in the po/
# directory at the top level. You a recent enough xgettext that supports
# --package-name
potfile=assets/po/luanti.pot
echo "updating pot"
xgettext --package-name=luanti \
	--add-comments='TRANSLATORS:'\
	--sort-by-file \
	--add-location=file \
	--keyword=N_ \
	--keyword=wgettext \
	--keyword=fwgettext \
	--keyword=fgettext \
	--keyword=fgettext_ne \
	--keyword=hgettext \
	--keyword=strgettext \
	--keyword=wstrgettext \
	--keyword=core.gettext \
	--keyword=showTranslatedStatusText \
	--keyword=fmtgettext \
	--output $potfile \
	--from-code=utf-8 \
	`find apps/game/src/ -name '*.cpp' -o -name '*.h'` \
	`find apps/game/src/builtin/ -name '*.lua'`

# Now iterate on all languages and create the po file if missing, or update it
# if it exists already
for lang in $langs ; do # note the missing quotes around $langs
	pofile=assets/po/$lang/luanti.po
	if test -e $pofile; then
		echo "[$lang]: updating strings"
		# Drop old strings *before* updating such that they can be re-used
		# until this script is run again.
		msgattrib --output-file=$pofile --no-obsolete $pofile
		msgmerge --update --backup=none --sort-by-file $pofile $potfile
	else
		# This will ask for the translator identity
		echo "[$lang]: NEW strings"
		msginit --locale=$lang --output-file=$pofile --input=$potfile
	fi

done
