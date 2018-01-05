#!/usr/bin/env bash

LIB_DIR_PATH=./ffmpeg/lib/
REPLACEABLE=~/Documents/github/ffmpeg/dist/lib
REPLACEMENT=@rpath

cd ${LIB_DIR_PATH} && echo "Goto dir $(pwd)"
rm -rf pkgconfig *.a && find . -type l -exec rm -rf {} \;
for path in `ls`; do
	if [[ ${path} =~ .*\.dylib ]]; then
		echo "install_name_tool -id ${REPLACEMENT}/$(basename ${path}) ${path}"
		install_name_tool -id ${REPLACEMENT}/$(basename ${path}) ${path}
		otool -L ${path} | grep -o -e ${REPLACEABLE}/.*dylib | while read line; do
			echo "install_name_tool -change ${line} ${REPLACEMENT}/${line##*/} ${path}"
			install_name_tool -change ${line} ${REPLACEMENT}/${line##*/} ${path}
		done
		symbol=${path%%.*}.${path##*.}
		if [[ ! -f ${symbol} ]]; then
            echo "ln -s ${path} ${symbol}"
		    ln -s ${path} ${symbol}
		fi
	fi
done
