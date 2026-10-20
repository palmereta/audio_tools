# Imagen para compilar y probar los plugins en Linux x86_64 (scripts/build-linux.sh).
# Ubuntu 22.04 a propósito: glibc 2.35, así los binarios corren en Ubuntu 22.04+, Debian 12+, Fedora 36+.
# Paquetes de external/JUCE/docs/Linux Dependencies.md, sin webkit (JUCE_WEB_BROWSER=0 en todos los targets).
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config ca-certificates curl zip unzip python3 file \
        libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
        libfreetype-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev \
        libegl-dev libglu1-mesa-dev mesa-common-dev \
        xvfb xauth fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
