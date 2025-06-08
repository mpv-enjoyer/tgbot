set -x

retVal=2
while [ $retVal -eq 2 ]
do
    git pull
    set -e
    docker build -t bot .
    set +e
    docker run --rm bot
    retVal=$?
done
