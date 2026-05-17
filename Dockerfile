# Prescient — reproducible LLVM 17 build environment.
# Mirrors what scripts/setup_env.sh provisions on a bare Ubuntu 22.04 host.
# Manual setup (setup_env.sh / build.sh / run.sh) remains fully supported;
# this image is an alternative, not a replacement.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /opt/prescient

# Provision the toolchain first so this layer stays cached across source
# edits. setup_env.sh installs LLVM 17, build tooling and Python deps.
COPY scripts/setup_env.sh scripts/setup_env.sh
COPY requirements.txt requirements.txt
RUN bash scripts/setup_env.sh

# Build the LLVM pass plugin (compiles IRComplexityEstimator.so).
COPY . .
RUN bash build.sh

# Default entry point is the pipeline runner; override the command to pick
# a mode, e.g.  docker run --rm prescient extract testcases/training/x.c
ENTRYPOINT ["./run.sh"]
CMD ["--help"]
