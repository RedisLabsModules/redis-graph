# BUILD redisfab/redisgraph:${VERSION}-${ARCH}-${OSNICK}

ARG REDIS_VER=6.0.9

# OSNICK=bionic|stretch|buster
ARG OSNICK=buster

# OS=debian:buster-slim|debian:stretch-slim|ubuntu:bionic
ARG OS=debian:buster-slim

# ARCH=x64|arm64v8|arm32v7
ARG ARCH=x64

#----------------------------------------------------------------------------------------------
FROM redisfab/redis:${REDIS_VER}-${ARCH}-${OSNICK} AS redis
# Build based on ${OS} (i.e., 'builder'), redis files are copies from 'redis'
FROM ${OS} AS builder

# Re-introducude arguments to this image
ARG OSNICK
ARG OS
ARG ARCH
ARG REDIS_VER

RUN echo "Building for ${OSNICK} (${OS}) for ${ARCH} [with Redis ${REDIS_VER}]"

WORKDIR /build

COPY --from=redis /usr/local/ /usr/local/

ADD ./ /build

# Set up a build environment
RUN ./deps/readies/bin/getpy3
RUN ./system-setup.py
RUN bash -l -c make

ARG TEST=0
ARG PACK=0

RUN set -ex ;\
	if [ "$TEST" = "1" ]; then bash -l -c "TEST= make test"; fi
RUN set -ex ;\
	if [ "$PACK" = "1" ]; then bash -l -c "make package"; fi

#---------------------------------------------------------------------------------------------- 
FROM redisfab/redis:${REDIS_VER}-${ARCH}-${OSNICK}

ARG OSNICK
ARG OS
ARG ARCH
ARG REDIS_VER
ARG PACK

ENV LIBDIR /usr/lib/redis/modules

WORKDIR /data

RUN mkdir -p $LIBDIR

COPY --from=builder /build/bin/artifacts/ /var/opt/redislabs/artifacts
COPY --from=builder /build/src/redisgraph.so $LIBDIR

RUN if [ ! -z $(command -v apt-get) ]; then apt-get -qq update; apt-get -q install -y libgomp1; fi
RUN if [ ! -z $(command -v yum) ]; then yum install -y libgomp; fi 

EXPOSE 6379
CMD ["redis-server", "--loadmodule", "/usr/lib/redis/modules/redisgraph.so"]
