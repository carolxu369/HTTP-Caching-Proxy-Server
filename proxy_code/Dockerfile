FROM ubuntu:latest
RUN mkdir /var/log/erss
ADD . /var/log/erss
RUN apt-get update && apt-get -y install gcc
WORKDIR /var/log/erss
CMD ["./proxy"]