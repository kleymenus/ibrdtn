#!/bin/sh
#
# convert uci configuration into daemon specific format
#

UCI=/sbin/uci

create_file() {
	echo "# -- DO NOT EDIT THIS FILE --" > $1
	echo "# automatic generated configuration file for IBR-DTN daemon" >> $1
	echo "#" >> $1
}

add_param() {
	VALUE=`$UCI -q get $2`
	
	if [ $? == 0 ]; then
		echo "$3 = $VALUE" >> $1
	fi
}

# create the file and write some header info
create_file $1

add_param $1 "ibrdtn.main.uri" "local_uri"
add_param $1 "ibrdtn.main.timezone" "timezone"
add_param $1 "ibrdtn.main.routing" "routing"
add_param $1 "ibrdtn.main.forwarding" "routing_forwarding"
add_param $1 "ibrdtn.main.blocksize" "limit_blocksize"

add_param $1 "ibrdtn.storage.blobs" "blob_path"
add_param $1 "ibrdtn.storage.bundles" "storage_path"
add_param $1 "ibrdtn.storage.limit" "limit_storage"

add_param $1 "ibrdtn.statistic.type" "statistic_type"
add_param $1 "ibrdtn.statistic.interval" "statistic_interval"
add_param $1 "ibrdtn.statistic.file" "statistic_file"

add_param $1 "ibrdtn.discovery.interface" "discovery_interface"
add_param $1 "ibrdtn.discovery.address" "discovery_address"

# iterate through all network interfaces
iter=0
netinterfaces=
while [ 1 == 1 ]; do
	$UCI -q get "ibrdtn.@network[$iter]" > /dev/null
	if [ $? == 0 ]; then
		netinterfaces="${netinterfaces} lan${iter}"
		add_param $1 "ibrdtn.@network[$iter].type" "net_lan${iter}_type"
		add_param $1 "ibrdtn.@network[$iter].interface" "net_lan${iter}_interface"
		add_param $1 "ibrdtn.@network[$iter].port" "net_lan${iter}_port"
	else
		break
	fi
	
	let iter=iter+1
done

# write list of network interfaces
echo "net_interfaces =$netinterfaces" >> $1

# iterate through all static routes
iter=0
while [ 1 == 1 ]; do
	$UCI -q get "ibrdtn.@static-route[$iter]" > /dev/null
	if [ $? == 0 ]; then
		PATTERN=`$UCI -q get "ibrdtn.@static-route[$iter].pattern"`
		DESTINATION=`$UCI -q get "ibrdtn.@static-route[$iter].destination"`
		let NUMBER=iter+1
		echo "route$NUMBER = $PATTERN $DESTINATION" >> $1
	else
		break
	fi
	
	let iter=iter+1
done

#iterate through all static connections
iter=0
while [ 1 == 1 ]; do
	$UCI -q get "ibrdtn.@static-connection[$iter]" > /dev/null
	if [ $? == 0 ]; then
		let NUMBER=iter+1
		add_param $1 "ibrdtn.@static-connection[$iter].uri" "static${NUMBER}_uri"
		add_param $1 "ibrdtn.@static-connection[$iter].address" "static${NUMBER}_address"
		add_param $1 "ibrdtn.@static-connection[$iter].port" "static${NUMBER}_port"
		add_param $1 "ibrdtn.@static-connection[$iter].protocol" "static${NUMBER}_proto"
	else
		break
	fi
	
	let iter=iter+1
done
