for i in `seq 1 $1`; do
    sed '2500 $d' inf_sub_$1.txt > inf_$1.txt
done

rm inf_sub_*