FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. && \
    make && \
    make install && \
    echo "✅ CSI logging library built and installed"

FROM ubuntu:24.04 AS csi-logging

RUN apt-get update && apt-get install -y \
    #libpthread-stubs0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/lib/libcsi_logging.so* /usr/local/lib/
COPY --from=builder /usr/local/include/csi_*.h /usr/local/include/

RUN ldconfig && \
    echo "✅ CSI logging library ready" && \
    ldd /usr/local/lib/libcsi_logging.so

WORKDIR /opt/csi-logging

CMD ["/bin/bash"]
