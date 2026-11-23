
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    curl \
    git \
    zip \
    unzip \
    tar \
    pkg-config \
    libssl-dev \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git
RUN /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy project and install dependencies
WORKDIR /app
COPY . .

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

RUN cmake --build build --config Release --target hallazgo_api -j2

# Runtime image
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/hallazgo_api /app/hallazgo_api
COPY --from=builder /app/clients/web /app/clients/web
COPY --from=builder /app/docs /app/docs

EXPOSE 18080

CMD ["/app/hallazgo_api", "/data/pg19/train", "/data/pg19/metadata.csv"]
