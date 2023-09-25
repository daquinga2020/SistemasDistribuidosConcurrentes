for i in `seq 1 $1`; do
WAIT=`printf '0.%06d\n' $RANDOM`;
(sleep $WAIT;./subscriber --ip $2 --port $3 --topic $4 > data_subs/fich_$i.txt) &
done