FROM ubuntu:24.04@sha256:a853f94d226358a79c740cfc7bce0c289748f3fe3488d921d038ccd752c61b60
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && \
    sed -i 's|http://archive.ubuntu.com/ubuntu/|https://snapshot.ubuntu.com/ubuntu/20261001T000000Z/|g; s|http://security.ubuntu.com/ubuntu/|https://snapshot.ubuntu.com/ubuntu/20261001T000000Z/|g' /etc/apt/sources.list.d/ubuntu.sources && \
    apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates python3 python3-matplotlib \
    latexmk lmodern texlive-latex-base texlive-latex-recommended texlive-latex-extra texlive-science \
    texlive-fonts-recommended poppler-utils ripgrep && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
