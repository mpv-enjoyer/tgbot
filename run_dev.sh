set -x

retVal=2
while [ $retVal -eq 2 ]
do
    set -e
    docker build -t bot .
    set +e
    docker run --rm -v ./data:/data -it bot /data/db
    retVal=$?
done
