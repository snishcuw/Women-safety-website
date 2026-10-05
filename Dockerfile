# ---- build stage ----
FROM gcc:13 AS build
WORKDIR /src
COPY safe_steps_styled.cpp .
RUN g++ -std=c++17 -O2 -pthread safe_steps_styled.cpp -o safe_steps

# ---- runtime stage ----
FROM debian:bookworm-slim
RUN useradd --system --no-create-home app
COPY --from=build /src/safe_steps /usr/local/bin/safe_steps
USER app
# Hosts like Render/Railway/Fly set $PORT; fall back to 8080 locally.
# --lan makes the server listen on 0.0.0.0 so the host's proxy can reach it.
CMD ["sh", "-c", "exec safe_steps ${PORT:-8080} --lan"]
