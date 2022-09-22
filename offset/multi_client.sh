
echo $1
if [[ $# < 1 ]] ;
then
	count=1
else
	count=$1
fi
echo $count

while [[ $count > 0 ]]
do
	nice -10 ./client &
	((count--))
	sleep 0.4
done
#read a
