#! /bin/bash
function nl_error {
  echo "build.sh: $*" >&2
  exit 1
}
function msg {
  echo "build.sh: $*" >&2
}

function print_usage {
cat  <<EOF
$0 [cross]
EOF
}

if [ "$1" = "--help" -o "$1" = "-h" ]; then
  print_usage
  exit 0
fi

crargs=''
if [ "$1" = "cross" ]; then
  shift
  crname='-cross'
else
  crname=''
fi

if [ -n "$1" ]; then
  msg "Unrecognized option: '$1'"
  print_usage
  exit 0
fi

subdir=''
relsrcroot='../git'
if [ ! -d .git ]; then
  # Not running in the source root
  while [ `pwd` != '/' -a ! -d .git ]; do
    subdir="/`basename $PWD`$subdir"
    relsrcroot="../$relsrcroot"
    cd ..
  done
  [ -d .git -a ../git -ef . ] || nl_error Unable to locate source root
  [ -f monarch-doxygen.css ] || nl_error Not in the correct source tree
fi
branch=`git rev-parse --abbrev-ref HEAD`
if [ -n "$crname" ]; then
  crargs=" -DCMAKE_TOOLCHAIN_FILE=$relsrcroot/arm-toolchain.cmake"
  crargs+=" -DCMAKE_STAGING_PREFIX=/opt/linkeng/am335x-$branch"
fi
builddir="../build$crname-$branch$subdir"
if [ ! -d $builddir ]; then
  msg Creating build $builddir
  mkdir -p $builddir
  cd $builddir
  cmake -DCMAKE_BUILD_TYPE=Debug $crargs $relsrcroot$subdir && make
else
  msg Using existing $builddir
  cd $builddir
  make
fi
