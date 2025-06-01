FROM reo7sp/tgbot-cpp

WORKDIR /usr/src
COPY . .
RUN cmake .
RUN make -j4
ENTRYPOINT ["/usr/src/bot"]