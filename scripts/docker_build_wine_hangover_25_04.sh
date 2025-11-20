#!/bin/bash
MATRIX_OS="ubuntu2504"
MATRIX_CODENAME="plucky"

HOVERSION="$(git describe --tags | sed "s/hangover-//")"
#echo "${HOVERSION}"

cp -r .packaging/${MATRIX_OS}/wine/* wine

cp wine/debian/changelog.org wine/debian/changelog 2>/dev/null || :

cp wine/debian/changelog wine/debian/changelog.old
cp wine/debian/changelog wine/debian/changelog.org
echo "hangover-wine (${HOVERSION}~${MATRIX_CODENAME}) UNRELEASED; urgency=low" > wine/debian/changelog.entry
echo "" >> wine/debian/changelog.entry
echo "  * Release ${HOVERSION}~${MATRIX_CODENAME}" >> wine/debian/changelog.entry
echo "" >> wine/debian/changelog.entry
echo -n " -- André Zwing <nerv@dawncrow.de>  " >> wine/debian/changelog.entry
LC_TIME=en_US.UTF-8 date "+%a, %d %b %Y %H:%M:%S %z" >> wine/debian/changelog.entry
echo "" >> wine/debian/changelog.entry
cat wine/debian/changelog.entry wine/debian/changelog.old > wine/debian/changelog
rm wine/debian/changelog.entry wine/debian/changelog.old
cat wine/debian/changelog

cd wine
docker build --no-cache -t wine${MATRIX_OS} .

#hangover-wine_10.14~plucky_arm64.deb 
NAME_OF_DEB="hangover-wine_${HOVERSION}~${MATRIX_CODENAME}_arm64.de"

#if an older manually instance of dummy is running...
docker rm -f dummy
docker create --name dummy wineubuntu2504
#docker cp dummy:/opt/hangover-wine_10.18~plucky_arm64.deb ./
docker cp dummy:/opt/${NAME_OF_DEB} ./
docker rm -f dummy