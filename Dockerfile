# syntax=docker/dockerfile:1
# cog0 standalone runtime image (RT5)
# Multi-stage: build Release standalone → slim runtime with binary only.

ARG BUILD_IMAGE=ubuntu:24.04
ARG RUNTIME_IMAGE=ubuntu:24.04

FROM ${BUILD_IMAGE} AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Standalone-first: no OpenCog deps
RUN cmake -S standalone -B /build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DCOGZERO_ENABLE_CPACK=OFF \
        -DCMAKE_INSTALL_PREFIX=/out \
    && cmake --build /build --parallel \
    && cmake --install /build \
    && /out/bin/cog0 --version

FROM ${RUNTIME_IMAGE} AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        ca-certificates \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 --shell /usr/sbin/nologin cog0

COPY --from=builder /out/ /usr/local/

# Optional service ports (documented; not started by default)
EXPOSE 8080 50051

USER cog0
WORKDIR /home/cog0

# Interactive REPL is a poor PID 1; default to version for health/smoke.
# Override: docker run ... cog0 --help | cog0 --demo | cog0 -c 5
ENTRYPOINT ["cog0"]
CMD ["--version"]

LABEL org.opencontainers.image.title="cog0" \
      org.opencontainers.image.description="Standalone Agent-Zero C++ runtime" \
      org.opencontainers.image.source="https://github.com/cogpy/cog-zero" \
      org.opencontainers.image.licenses="AGPL-3.0" \
      org.opencontainers.image.vendor="cogpy"
