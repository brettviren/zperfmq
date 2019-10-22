#!/usr/bin/env bash
set -ex

# NOTE: This script is not standalone, it is included from project root
# ci_build.sh script, which sets some envvars (like REPO_DIR below).
[ -n "${REPO_DIR-}" ] || exit 1

# Verify all required dependencies with repos can be checked out
# Note: certain zproject scripts that deal with deeper dependencies expect that
# such checkouts are directly in the same parent directory as "this" project.
cd "$REPO_DIR/.."
git clone --quiet --depth 1 https://github.com/zeromq/libzmq.git libzmq
git clone --quiet --depth 1 https://github.com/zeromq/czmq.git czmq
git clone --quiet --depth 1 https://github.com/zeromq/zyre.git zyre
cd -

if ! ((command -v dpkg-query >/dev/null 2>&1 && dpkg-query --list zproject >/dev/null 2>&1) || \
       (command -v brew >/dev/null 2>&1 && brew ls --versions zproject >/dev/null 2>&1)); then
    cd "$REPO_DIR/.."
    git clone --quiet --depth 1 https://github.com/zeromq/zproject zproject
    cd zproject
    PATH="`pwd`:$PATH"
fi

if ! ((command -v dpkg-query >/dev/null 2>&1 && dpkg-query --list generator-scripting-language >/dev/null 2>&1) || \
       (command -v brew >/dev/null 2>&1 && brew ls --versions gsl >/dev/null 2>&1)); then
    cd "$REPO_DIR/.."
    git clone https://github.com/zeromq/gsl.git gsl
    cd gsl/src
    make
    PATH="`pwd`:$PATH"
fi
export PATH

# Verify that zproject template is up-to-date with files it can overwrite
# As we will overwrite this script file make sure bash loads the
# next lines into memory before executing
# http://stackoverflow.com/questions/21096478/overwrite-executing-bash-script-files
{
    cd "$REPO_DIR"
    gsl project.xml

    # keep an eye on git version used by CI
    git --version
    if [[ $(git --no-pager diff -w) ]]; then
        git --no-pager diff -w
        echo "There are diffs between current code and code generated by zproject!"
        exit 1
    fi
    if [[ $(git status -s) ]]; then
        git status -s
        echo "zproject generated new files!"
        exit 1
    fi
    exit 0
}
