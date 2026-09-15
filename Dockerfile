ARG UBUNTU_IMAGE=ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254

FROM ${UBUNTU_IMAGE} AS toolchain
RUN apt-get update \
    && apt-get install -y --no-install-recommends clang make libc6-dev python3 procps \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app

FROM toolchain AS build
COPY Makefile ./
COPY include/ include/
COPY src/ src/
RUN make -j2

FROM build AS test
COPY tests/ tests/
RUN mkdir -p test-results && chown -R 10001:10001 /app
USER 10001:10001
CMD ["make", "test"]

FROM ${UBUNTU_IMAGE} AS runtime
RUN groupadd --gid 10001 httpserver \
    && useradd --uid 10001 --gid 10001 --no-create-home --home-dir /data --shell /usr/sbin/nologin httpserver \
    && install -d -o 10001 -g 10001 -m 0700 /data
COPY --from=build /app/build/httpserver /usr/local/bin/httpserver
USER 10001:10001
WORKDIR /data
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/httpserver"]
CMD ["-t", "4", "8080"]
