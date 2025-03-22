# syntax=docker/dockerfile:1
ARG OS_VERSION=22.04

FROM ubuntu:$OS_VERSION
ENV DEBIAN_FRONTEND=noninteractive

ENV CONFIG="/5g-sniffer/MSU-Private5G184205.toml"

RUN apt -y update
RUN apt -y install git cmake make gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev libliquid-dev libconfig++-dev libzmq3-dev libspdlog-dev libfmt-dev libuhd-dev uhd-host clang

WORKDIR /5g-sniffer

COPY . .

RUN mkdir -p build && rm -rf build/*

WORKDIR /5g-sniffer/build

RUN cmake ..

RUN make -j$(nproc) && make install && ldconfig

CMD ["sh", "-c", "5g-sniffer /sniffer.toml"]
