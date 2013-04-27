#! /bin/sh

#modprobe pktgen


pgset() {
    local result

    echo $1 > $PGDEV

    result=`cat $PGDEV | fgrep "Result: OK:"`
    if [ "$result" = "" ]; then
        cat $PGDEV | fgrep Result:
    fi
}

pg() {
    echo inject > $PGDEV
    cat $PGDEV
}

# Config Start Here -----------------------------------------------------------


# thread config
# Each CPU has own thread. Two CPU exammple. We add eth0, eth2 respectivly.

PGDEV=/proc/net/pktgen/kpktgend_0
echo "Removing all devices"
pgset "rem_device_all"
echo "Adding eth0"
pgset "add_device eth0"
echo "Setting max_before_softirq 10000"
pgset "max_before_softirq 10000"


# device config
# ipg is inter packet gap. 0 means maximum speed.

CLONE_SKB="clone_skb 0"
# NIC adds 4 bytes CRC
PKT_SIZE="pkt_size 60"

# COUNT 0 means forever
#COUNT="count 0"
COUNT="count 10"  # send 10 packets
IPG="ipg 0"

PGDEV=/proc/net/pktgen/eth0
echo "Configuring $PGDEV"
pgset "$COUNT"
pgset "$CLONE_SKB"
pgset "$PKT_SIZE"
#pgset "$IPG"
pgset "dst 127.0.0.1"
pgset "dst_mac  0A:0A:0A:0A:0A:0A"
pgset "src_min 10.0.0.1"           # Set the minimum (or only) source IP.
pgset "src_max 10.0.0.254"         # Set the maximum source IP.
pgset "udp_src_min 8088"

# Time to run
PGDEV=/proc/net/pktgen/pgctrl

echo "Running... ctrl^C to stop"
pgset "start"
echo "Done"

# Result can be vieved in /proc/net/pktgen/eth0
