# syntax=docker/dockerfile:1
ARG OS_VERSION=22.04

FROM ubuntu:$OS_VERSION
ENV DEBIAN_FRONTEND=noninteractive

ENV CONFIG="/replay-agent/MSU-Private5G184205.toml"

RUN apt -y update
RUN apt -y install git cmake make gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev libliquid-dev libconfig++-dev libzmq3-dev libspdlog-dev libfmt-dev libuhd-dev uhd-host clang

WORKDIR /replay-agent

COPY . .

RUN mkdir -p build && rm -rf build/*

WORKDIR /replay-agent/build

ENV CXX /usr/bin/clang++-14
ENV CC /usr/bin/clang-14

RUN cmake -DCMAKE_C_COMPILER=/usr/bin/clang-14 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-14 ..
#RUN make -j$(nproc)

#CMD ["sh", "-c", "/5gsniffer/5gsniffer/build/src/5g_sniffer \"$CONFIG\""]
CMD ["/bin/bash"]
