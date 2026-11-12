#!/bin/bash
 
docker build . -t "ariadne-$1" --no-cache
docker run \
    --name "ariadne-container-$1" \
    "ariadne-$1" pwd

docker cp "ariadne-container-$1":ariadne/build/results.csv ./results/
docker rm --force "ariadne-container-$1"
docker image rm --force "ariadne-$1"
