FROM alpine

RUN apk update && apk add -- git openssh g++ cmake make python bash boost-dev

RUN git config --global http.sslVerify false

COPY build_cachelotd.sh /bin/build_cachelotd.sh
RUN chmod ugo+rx /bin/build_cachelotd.sh
VOLUME "/cachelot"
RUN rm -rf /cachelot/* && mkdir -p /cachelot/src
RUN adduser -D -h /cachelot -s /bin/bash cachelot
RUN chown -R cachelot:cachelot /cachelot && chmod ug+rwx /cachelot
USER cachelot
CMD build_cachelotd.sh
WORKDIR "/cachelot"

