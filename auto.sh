set -e

rm -rf build
mkdir -p build
cd build
cmake ..
make -j `nproc`
./xiaozhi_qt
