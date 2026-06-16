FROM ubuntu:22.04

ARG TARGETARCH
ARG VERSION
ARG REVISION

LABEL org.opencontainers.image.title="Aria2 Next"
LABEL org.opencontainers.image.description="Maintained aria2-compatible download engine."
LABEL org.opencontainers.image.source="https://github.com/AnInsomniacy/aria2-next"
LABEL org.opencontainers.image.url="https://github.com/AnInsomniacy/aria2-next"
LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.revision="${REVISION}"
LABEL org.opencontainers.image.licenses="GPL-2.0-only"

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      ca-certificates \
      libssl3 \
      libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY "linux-${TARGETARCH}/aria2-next" /usr/local/bin/aria2-next
COPY aria2.conf /usr/share/aria2-next/aria2.conf
COPY entrypoint.sh /usr/local/bin/aria2-next-entrypoint

RUN set -eux; \
    groupadd --system --gid 10001 aria2next; \
    useradd --system --uid 10001 --gid aria2next --home-dir /var/lib/aria2-next --shell /usr/sbin/nologin aria2next; \
    mkdir -p /downloads /config /var/lib/aria2-next /usr/share/aria2-next; \
    touch /var/lib/aria2-next/session.txt; \
    chown -R aria2next:aria2next /downloads /config /var/lib/aria2-next; \
    chmod 755 /usr/local/bin/aria2-next; \
    chmod 755 /usr/local/bin/aria2-next-entrypoint; \
    chmod 644 /usr/share/aria2-next/aria2.conf
RUN /usr/local/bin/aria2-next --version >/dev/null

WORKDIR /downloads
VOLUME ["/downloads", "/config", "/var/lib/aria2-next"]
EXPOSE 6800
USER aria2next
ENTRYPOINT ["aria2-next-entrypoint"]
CMD ["--conf-path=/config/aria2.conf"]
