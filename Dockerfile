# Build tiny static binary on musl
FROM alpine:3.22 AS build
RUN apk add --no-cache musl-dev build-base upx
WORKDIR /src
COPY main.c .
RUN cc -Os -s -static -fdata-sections -ffunction-sections -Wl,--gc-sections -o app main.c && upx --best --lzma app

# Final image: just the binary
FROM scratch
COPY --from=build /src/app /app
EXPOSE 8080
USER 10001
ENTRYPOINT ["/app"]
