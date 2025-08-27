FROM alpine:3.22 AS build
RUN apk add --no-cache build-base upx
WORKDIR /src
COPY main.c lyrics.h sys.h notstdlib.h .
RUN cc -Os -s -static -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables \
       -fomit-frame-pointer -fno-pic -no-pie -ffunction-sections -fdata-sections \
       -Wl,--gc-sections -Wl,--build-id=none -Wl,-e,_start \
       -o app main.c && \
    strip --strip-all -R .comment -R .note.* -R .gnu* app

FROM scratch
COPY --from=build /src/app /app
COPY diggy.conf .
EXPOSE 8080
USER 10001

# Image size and metadata labels
LABEL image.size="small" \
      image.size.description="Minimalistic HTTP server with Docker image size less than 100kB" \
      image.size.target="<100kB" \
      org.opencontainers.image.title="Diggy HTTP Server" \
      org.opencontainers.image.description="Extremely minimalistic, static-route HTTP server with simple config via file, env, or CLI. Docker image size less than 100kB." \
      org.opencontainers.image.vendor="lynxzp" \
      org.opencontainers.image.source="https://github.com/lynxzp/diggy"

ENV DIGGY_PORT=8080
ENV DIGGY_HOST=0.0.0.0
ENV DIGGY_INTERVAL_MS=1800
ENV DIGGY_POLL_TIMEOUT_MS=100
ENV DIGGY_MINE=1

ENTRYPOINT ["/app", "-port=8080"]
