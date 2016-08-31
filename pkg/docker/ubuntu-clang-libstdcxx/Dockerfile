FROM ubuntu:14.04
MAINTAINER Cachelot Team <cachelot@cachelot.io>

RUN cat /dev/null > /etc/apt/sources.list \
    && echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty-updates main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty-backports main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty-security main restricted universe multiverse" >> /etc/apt/sources.list

RUN apt-get update -qq && apt-get install -qq
RUN apt-get install -qq --no-install-recommends  \
    git \
    ssh \
    wget \
    python \
    subversion \
    cmake \
    make \
    libboost-system1.55-dev libboost-test1.55-dev libboost-program-options1.55-dev \
    libstdc++-4.8-dev \
    && rm -rf /var/lib/apt/lists/*

# LLVM/Clang
RUN wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
RUN echo "deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty main" >> /etc/apt/sources.list
RUN echo "deb-src http://llvm.org/apt/trusty/ llvm-toolchain-trusty main" >> /etc/apt/sources.list
RUN apt-get update -qq
RUN apt-get install -qq clang-3.8

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 0
RUN update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 0
RUN update-alternatives --set clang /usr/bin/clang-3.8
RUN update-alternatives --set clang++ /usr/bin/clang++-3.8
ENV CC="clang"
ENV CXX="clang++"
ENV CXXFLAGS="-std=c++11 -stdlib=libstdc++"
ENV LDFLAGS="-stdlib=libstdc++"

# Build compiler-rt with the AddressSanizier library
RUN D=$(pwd) \
    && NUM_CPUS=$(grep -c '^processor' /proc/cpuinfo) \
    && cd ~ \
    && svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm > /dev/null \
    && cd llvm \
    && R=$(svn info | grep Revision: | awk '{print $2}') \
    && (cd tools && svn co -r $R http://llvm.org/svn/llvm-project/cfe/trunk clang > /dev/null) \
    && (cd projects && svn co -r $R http://llvm.org/svn/llvm-project/compiler-rt/trunk compiler-rt > /dev/null) \
    && mkdir llvm_cmake_build && cd llvm_cmake_build \
    && CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_WERROR=ON -DLLVM_TARGETS_TO_BUILD=X86 ~/llvm > /dev/null \
    && make -j${NUM_CPUS} \
    && make install \
    && cd $D \
    && rm -rf ~/llvm

RUN apt-get clean


ENV ASAN_SYMBOLIZER_PATH="/usr/lib/llvm-*/bin/llvm-symbolizer"
ENV ASAN_OPTIONS=symbolize=1

WORKDIR "/cachelot"
COPY run_cachelot_tests.sh /bin/run_cachelot_tests.sh
RUN chmod a+x /bin/run_cachelot_tests.sh
ENTRYPOINT run_cachelot_tests.sh
