cd ..
make clean
./autogen.sh
./contrib/configure-release --prefix=/where/to/install
./contrib/configure-devel --prefix=$PWD/install-debug
make -j8
make install

