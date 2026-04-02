#!/bin/bash
#docker run --mount type=bind,source="$(pwd)",target="/usr/src/app" -it emscripten/emsdk:3.1.69 bash

SKIP_JS=${SKIP_JS:-}
CIMBAR_ROOT=${CIMBAR_ROOT:-/usr/src/app}
OPENCV_DIR=${OPENCV_DIR:-$CIMBAR_ROOT/opencv4}
EMSCRIPTEN_DIR=${EMSCRIPTEN_DIR:-/emsdk/upstream/emscripten}
cd $CIMBAR_ROOT

apt update
apt install python3 git -y

if [ ! -d "$OPENCV_DIR" ]; then
	git clone --depth 1 --branch 4.10.0 https://github.com/opencv/opencv.git "$OPENCV_DIR"
fi

cd "$OPENCV_DIR"
mkdir -p opencv-build-wasm
cd opencv-build-wasm
python3 ../platforms/js/build_js.py build_wasm --build_wasm --emscripten_dir="$EMSCRIPTEN_DIR"

cd $CIMBAR_ROOT
mkdir -p build-wasm
cd build-wasm
emcmake cmake .. -DUSE_WASM=1 -DOPENCV_DIR="$OPENCV_DIR"
make -j5 install
(cd ../web/ && bash wasmgz.sh)

if [ -n "$SKIP_JS" ]; then
	echo "early exit"
	exit 0
fi

cd $CIMBAR_ROOT
mkdir -p build-asmjs
cd build-asmjs
emcmake cmake .. -DUSE_WASM=2 -DOPENCV_DIR="$OPENCV_DIR"
make -j5 install
(cd ../web/ && zip cimbar.asmjs.zip cimbar_js.js index.html main.js)

(cd $CIMBAR_ROOT && python3 package-cimbar-html.py)

