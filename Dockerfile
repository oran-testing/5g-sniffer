# syntax=docker/dockerfile:1
ARG OS_VERSION=22.04

FROM ubuntu:$OS_VERSION
ENV DEBIAN_FRONTEND=noninteractive

ENV CONFIG="/5gsniffer/5gsniffer/MSU-Private5G184205.toml"

WORKDIR /5gsniffer
RUN apt -y update
RUN apt -y install git cmake make gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev libliquid-dev libconfig++-dev libzmq3-dev libspdlog-dev libfmt-dev libuhd-dev uhd-host clang

COPY . .

WORKDIR /5gsniffer/5gsniffer
RUN mkdir -p build

WORKDIR /5gsniffer/5gsniffer/build

ENV CXX /usr/bin/clang++-14
ENV CC /usr/bin/clang-14

RUN cmake -DCMAKE_C_COMPILER=/usr/bin/clang-14 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-14 ..
RUN make -j$(nproc)

#ENTRYPOINT ["/5gsniffer/5gsniffer/build/src/5gsniffer", "$CONFIG"]
CMD ["sh", "-c", "/5gsniffer/5gsniffer/build/src/5g_sniffer \"$CONFIG\""]
