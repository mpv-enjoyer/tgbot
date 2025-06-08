FROM reo7sp/tgbot-cpp

RUN apt-get -y update
RUN apt-get -y install libsqlite3-dev

WORKDIR /usr/src
COPY . .
RUN cmake .
RUN make -j4
ENTRYPOINT ["/usr/src/bot"]