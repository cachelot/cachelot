FROM alpine
MAINTAINER Cachelot Team <cachelot@cachelot.io>
RUN apk update \
    && apk add --no-cache libstdc++ \
    && rm -rf {"/var/cache/apk/*","/var/lib/apt/lists/*"}
COPY cachelotd /usr/bin/cachelotd
COPY dumb-init /usr/bin/dumb-init
RUN adduser -S cachelot
RUN chmod ugo+rx /usr/bin/cachelotd
USER cachelot
ENTRYPOINT [ "/usr/bin/dumb-init", "--single-child", "--", "/usr/bin/cachelotd" ]
EXPOSE 11211
