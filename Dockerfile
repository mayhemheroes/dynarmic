# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y cmake clang ninja-build

## Install aft++
#RUN DEBIAN_FRONTEND=noninteractive apt-get install afl

## Add source code to the build stage.
ADD . /dynarmic 
WORKDIR /dynarmic/externals/fmt/test/fuzzing/

## TODO: ADD YOUR BUILD INSTRUCTIONS HERE.
RUN mkdir build-fuzzers-libfuzzer
RUN export CXX=clang++
RUN ./build_libfuzz.sh

# Package Stage
FROM --platform=linux/amd64 ubuntu:20.04

## TODO: Change <Path in Builder Stage>
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/chrono-duration-fuzzer /
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/chrono-timepoint-fuzzer /
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/float-fuzzer /
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/named-arg-fuzzer /
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/one-arg-fuzzer /
COPY --from=builder /dynarmic/externals/fmt/test/fuzzing/build-fuzzers-libfuzzer/bin/two-args-fuzzer /

