#!/usr/bin/env bash

function recursive_sed {
    find "$2" -type f -exec sed -i "$1" {} +
}

if [ $# -lt 1 ]; then
    echo "usage: ./standalonify-beast.sh <beast source>"
    exit
fi

beast_root_dir="$1"

files=$(ls $beast_root_dir)
if ! grep --quiet "CODE_OF_CONDUCT.md" <<< $files \
    && ! grep --quiet "CONTRIBUTING.md" <<< $files \
    && ! grep --quiet "CHANGELOG.md" <<< $files \
    && ! grep --quiet "LICENSE_1_0.txt" <<< $files \
    && ! grep --quiet "include" <<< $files \
    && ! grep --quiet "example" <<< $files \
    && ! grep --quiet "meta" <<< $files \
    && ! grep --quiet "test" <<< $files \
    && ! grep --quiet "tools" <<< $files \
    && ! grep --quiet "doc" <<< $files \
    && ! grep --quiet "index.html" <<< $files
then
    echo "$beast_root_dir is not the Beast root directory."
    exit
fi

echo "Moving Beast out of the Boost namespace..."
recursive_sed 's/<boost\/beast/<beast/g' $beast_root_dir
recursive_sed 's/beast/beast/g' $beast_root_dir
recursive_sed 's/BEAST/BEAST/g' $beast_root_dir
recursive_sed 's/ASIO/ASIO/g' $beast_root_dir
recursive_sed '/} \/\/ boost/d' $beast_root_dir
mv $beast_root_dir/include/boost/beast{,.hpp} $beast_root_dir/include/
rmdir $beast_root_dir/include/boost

echo "Converting Asio includes..."
recursive_sed 's/<boost\/asio/<asio/g' $beast_root_dir

echo "Converting Asio namespace..."
recursive_sed 's/asio/asio/g' $beast_root_dir

echo "Converting the namespace through which error_code, errc, and system_error are accessed..."
recursive_sed 's/std::/std::/g' $beast_root_dir

beast_path=$beast_root_dir/include/beast
error_path=$beast_path/core/error.hpp

echo "Changing the inclues in error.hpp to include <system_error> instead of the Boost equivalent..."
# TODO change to recursive_sed?
sed -i 's/<boost\/system\/error_code.hpp>/<system_error>/g' $error_path
sed -i '/#include <boost\/system\/system_error.hpp>/d' $error_path

echo "Changing the aliases in error.hpp to alias the std implementation instead of boost::system..."
sed -i 's/boost::system/std/g' $error_path
sed -i 's/namespace errc = /using /g' $error_path

echo "Adding std:: prefix to std::is_nothrow_move_constructibe..."
recursive_sed 's/std::is_nothrow_move/std::std::is_nothrow_move/g' $beast_root_dir

// TODO probably best not to be so fine-grained as new error_codes could be
// added any time
echo "Moving the is_error_code_enum struct from the boost::system namespace to the std namespace..."
for file in $beast_path/{http,websocket,zlib}/impl/error.ipp; do
    sed -i 's/namespace system {/namespace std {/g' $file 
    sed -i 's/} \/\/ system/} \/\/ std/g' $file 
done

echo "Changing manual error_code assignments..."
recursive_sed 's/\(\b.*\b\)\.assign(\(errc::[a-z_0-9]\+\), [a-z_0-9]\+())/\1 = std::make_error_code(\2)/g' $beast_root_dir


# TODO move beast/test/extras/boost/beast out of the boost directory
