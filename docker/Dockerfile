### Build/test container ###
# Define builder stage
FROM myserver:base as builder

# Share work directory
COPY . /usr/src/projects/big-fat-dinosours-w-short-front-legs
WORKDIR /usr/src/projects/big-fat-dinosours-w-short-front-legs/build

# Build and test
RUN cmake ..
RUN make
RUN ctest --output-on_failure


### Deploy container ###
# Define deploy stage
FROM ubuntu:noble as deploy

# Copy server output binary to "."
COPY --from=builder /usr/src/projects/big-fat-dinosours-w-short-front-legs/build/bin/webserver .
COPY --from=builder /usr/src/projects/big-fat-dinosours-w-short-front-legs/tests/my_config ./my_config

# Expose port
EXPOSE 80

# Use ENTRYPOINT to specify the binary name
ENTRYPOINT ["./webserver"]

# Use CMD to specify arguments to ENTRYPOINT
CMD ["my_config"]
