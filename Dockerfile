FROM devkitpro/devkitarm:20241104

COPY docker/sources.list /etc/apt/sources.list
RUN apt-get update && apt-get install -y --no-install-recommends gcc g++ libc6-dev

ADD https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz /tmp/libiconv.tar.gz
RUN tar -xzf /tmp/libiconv.tar.gz -C /tmp && \
    cd /tmp/libiconv-1.18 && \
    CC=/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc \
    CFLAGS="-O2 -mthumb -mthumb-interwork -march=armv5te -ffunction-sections -fdata-sections" \
    ./configure --host=arm-none-eabi --prefix=/opt/np3d/libiconv --disable-shared --disable-nls && \
    make lib/localcharset.h && \
    make -C lib && make -C lib install && \
    make -C libcharset/lib && make -C libcharset/lib install && \
    install -D -m 644 include/iconv.h.inst /opt/np3d/libiconv/include/iconv.h
