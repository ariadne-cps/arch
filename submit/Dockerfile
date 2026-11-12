FROM ubuntu:jammy
MAINTAINER Luca Geretti EMAIL luca.geretti@univr.it
ENV TZ=Europe/Rome
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt update && apt install -y cmake git g++-12 pkg-config libmpfr-dev libcairo2-dev
RUN git clone https://github.com/ariadne-cps/ariadne ariadne
RUN cd ariadne && git checkout arch2026 && mkdir build 
RUN chmod -R 777 ./ariadne
WORKDIR "/ariadne/build"
RUN cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12
RUN make arch_suite
RUN experimental/examples/arch/arch_suite
