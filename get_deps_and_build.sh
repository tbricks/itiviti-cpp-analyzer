[ ! -d thd ] && mkdir thd
cd thd

if [ ! -d boost ]; then
    svn co svn://svn.tbricks.com/thd/tbricks-2.14.7/.build.cache/build.x86_64-unknown-linux.64/ boost --depth empty
    cd boost
    svn up boost-1.68.0.gtar.gz
    tar -xzf boost-1.68.0.gtar.gz
    rm boost-1.68.0.gtar.gz
    cd ..
fi

cd ..
BOOST=`pwd`/thd/boost ./do_build.sh