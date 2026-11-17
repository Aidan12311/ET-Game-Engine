FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

ENV WORLD_JSON_PATH=/app/shared/static/world.json \
    TILESET_DIR_PATH=/app/shared/tileset_metadata \
    SAVE_DIR_PATH=/app/saves

EXPOSE 8001

CMD ["./build/et_game_service"]
