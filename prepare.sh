#!/bin/bash -e

set -e

echo This script downloads the code for the benchmarks
echo It will also attempt to build the benchmarks
echo It will output OK at the end if builds succeed
echo
IOR_HASH=68f51e1fb8e8d11f54f9de47d5265bdb3da3d644
#IOR_HASH=14deedfec48ce295dff683d15c1b194652bd6d08
PFIND_HASH=62c3a7e31

INSTALL_DIR=$PWD
BIN=$INSTALL_DIR/bin
BUILD=$PWD/build
MAKE="make -j4"

function main {
  # listed here, easier to spot and run if something fails
  setup

  get_schema_tools
  get_ior
  get_pfind

  build_ior
  build_pfind
  build_io500

  echo
  echo "OK: All required software packages are now prepared"
  ls $BIN
}

function setup {
  #rm -rf $BUILD $BIN
  mkdir -p $BUILD $BIN
  #cp utilities/find/mmfind.sh $BIN
}

function git_co {
  local repo=$1
  local dir=$2
  local tag=$3

  pushd $BUILD
  [ -d "$dir" ] || git clone $repo $dir
  cd $dir
  git fetch
  git checkout $tag
  popd
}

###### GET FUNCTIONS
function get_ior {
  echo "Getting IOR and mdtest"
  git_co git@github.com:JarvanLaw/ior.git ior $IOR_HASH
  #git_co https://github.com/hpc/ior.git ior $IOR_HASH
}

function get_pfind {
  echo "Preparing parallel find"
  git_co git@github.com:VI4IO/pfind.git pfind $PFIND_HASH
  #git_co https://github.com/VI4IO/pfind.git pfind $PFIND_HASH
}

function get_schema_tools {
  echo "Downloading supplementary schema tools"
  git_co git@github.com:VI4IO/cdcl-schema-tools.git cdcl-schema-tools
  #git_co https://github.com/VI4IO/cdcl-schema-tools.git cdcl-schema-tools
  [ -d "$dir" ] || ln -sf $PWD/build/cdcl-schema-tools  schema-tools
}

###### BUILD FUNCTIONS
function build_ior {
  pushd $BUILD/ior
  ./bootstrap
  # Add here extra flags
  ./configure --prefix=$INSTALL_DIR --with-gramfs
  cd src
  $MAKE clean
  $MAKE install
  echo "IOR: OK"
  echo
  popd
}

function build_pfind {
  pushd $BUILD/pfind
  ./prepare.sh
  ./compile.sh
  ln -sf $BUILD/pfind/pfind $BIN/pfind
  echo "Pfind: OK"
  echo
  popd
}

function build_io500 {
  make
  echo "io500: OK"
  echo
}

###### CALL MAIN
main
