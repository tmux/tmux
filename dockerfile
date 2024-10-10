FROM debian:stable

RUN apt update && apt install -y \
gcc \
bison \
autoconf \
pkgconf \
make \
libevent-dev \
libncurses-dev

RUN useradd app

RUN mkdir /tmux-bin
RUN chown app:app /tmux-bin

RUN mkdir /tmux
RUN chown app:app /tmux

WORKDIR /tmux

USER app

ENV PATH="$PATH:/tmux-bin"

CMD [ "bash" ]
