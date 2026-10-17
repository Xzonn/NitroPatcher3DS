FROM devkitpro/devkitarm:20241104

COPY docker/sources.list /etc/apt/sources.list
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev
