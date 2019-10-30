FROM ubuntu:latest

RUN apt-get update
RUN apt-get install -y g++ --fix-missing

ADD ./src /src/
WORKDIR /src/
RUN g++ udp.cpp -o udp -pthread
ENTRYPOINT ["/src/udp"]
