[![CI-Release](https://github.com/wiiu-env/RPXLoadingModule/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/RPXLoadingModule/actions/workflows/ci.yml)

## Usage
(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file `RPXLoadingModule.wms` into `sd:/wiiu/environments/[ENVIRONMENT]/modules`.  
2. Requires the [WUMSLoader](https://github.com/wiiu-env/WUMSLoader) in `sd:/wiiu/environments/[ENVIRONMENT]/modules/setup`.
3. Requires the [ContentRedirectionModule](https://github.com/wiiu-env/ContentRedirectionModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules/setup`.
4. Requires the [WUHBUtilsModule](https://github.com/wiiu-env/WUHBUtilsModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules/setup`.
5. Use [librpxloading](https://github.com/wiiu-env/librpxloader)

## Buildflags

### Logging
Building via `make` only logs errors (via OSReport). To enable logging via the [LoggingModule](https://github.com/wiiu-env/LoggingModule) set `DEBUG` to `1` or `VERBOSE`.

`make` Logs errors only (via OSReport).  
`make DEBUG=1` Enables information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).  
`make DEBUG=VERBOSE` Enables verbose information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).

If the [LoggingModule](https://github.com/wiiu-env/LoggingModule) is not present, it'll fallback to UDP (Port 4405) and [CafeOS](https://github.com/wiiu-env/USBSerialLoggingModule) logging.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t rpxloadingmodule-builder

# make 
docker run -it --rm -v ${PWD}:/project rpxloadingmodule-builder make

# make clean
docker run -it --rm -v ${PWD}:/project rpxloadingmodule-builder make clean
```

## Format the code via docker

`docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./src -i`
