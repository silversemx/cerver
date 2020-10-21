FROM gcc as builder

WORKDIR /opt/cerver

RUN apt-get update && apt-get install -y cmake libssl-dev

# cerver
COPY . .
RUN make -j4 && make examples

############
FROM ubuntu:bionic

ARG RUNTIME_DEPS='libssl-dev'

RUN apt-get update && apt-get install -y ${RUNTIME_DEPS} && apt-get clean

# install cerver
COPY --from=builder /opt/cerver/bin/libcerver.so /usr/local/lib/
COPY --from=builder /opt/cerver/include/cerver /usr/local/include/cerver

RUN ldconfig

# cerver examples
WORKDIR /home/cerver
COPY --from=builder /opt/cerver/examples/bin ./examples

CMD ["./examples/test"]