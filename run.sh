set -xe

retVal=2
if [ $retVal -eq 2 ]; then
    git pull
    docker build -t bot .
    docker run --rm -it bot
    retVal=$?
fi
