FROM ubuntu:22.04

ARG TARGETARCH
ARG VERSION
ARG REVISION

LABEL org.opencontainers.image.title="Aria2 Next"
LABEL org.opencontainers.image.description="Maintained multi-protocol download engine with libtorrent-backed BitTorrent."
LABEL org.opencontainers.image.source="https://github.com/AnInsomniacy/aria2-next"
LABEL org.opencontainers.image.url="https://github.com/AnInsomniacy/aria2-next"
LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.revision="${REVISION}"
LABEL org.opencontainers.image.licenses="GPL-2.0-only"

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      ca-certificates \
      gosu \
      libssl3 \
      libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY "linux-${TARGETARCH}/aria2-next" /usr/local/bin/aria2-next
COPY aria2.conf /usr/share/aria2-next/aria2.conf
COPY entrypoint.sh /usr/local/bin/aria2-next-entrypoint

RUN set -eux; \
    mkdir -p /downloads /config /var/lib/aria2-next /usr/share/aria2-next; \
    chmod 755 /usr/local/bin/aria2-next; \
    chmod 755 /usr/local/bin/aria2-next-entrypoint; \
    chmod 644 /usr/share/aria2-next/aria2.conf
RUN /usr/local/bin/aria2-next --version >/dev/null

# Tools run inside the container print and read file names as UTF-8.
ENV LANG=C.UTF-8

WORKDIR /downloads
EXPOSE 6800
USER root
ENTRYPOINT ["aria2-next-entrypoint"]
CMD ["--conf-path=/config/aria2.conf"]
