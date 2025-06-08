### coverage.Dockerfile
FROM myserver:base as coverage

# Share work directory
COPY . /usr/src/projects/big-fat-dinosours-w-short-front-legs
WORKDIR /usr/src/projects/big-fat-dinosours-w-short-front-legs/build

# Build and test
RUN cmake ..
RUN make
RUN ctest --output-on_failure

# Generate coverage report
RUN gcovr -r .. --html --html-details -o coverage_report.html