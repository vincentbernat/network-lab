#!/bin/sh

cat <<EOF > /dev/null
# To provision:
  mkdir /mnt/lab
  mount -t msdosfs /dev/da0 /mnt/lab
  sh /mnt/lab/run.sh
EOF

cd $(dirname $0)

# dhclient is running on all interfaces
pkill dhclient

# Configure SSH access
mkdir ~/.ssh
chmod 700 ~/.ssh
cat id_rsa.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
echo PermitRootLogin without-password >> /etc/ssh/sshd_config
service sshd onestart

# Shell
env ASSUME_ALWAYS_YES=YES pkg install zsh curl mg subversion
curl -s https://vincentbernat-zshrc.s3.amazonaws.com/zsh-install.sh | sh

echo 'gateway_enable="YES"' >> /etc/rc.conf
service routing restart
route add -net 0.0.0.0/0 10.0.2.2

ifconfig lo0 inet 203.0.113.14/32 alias
ifconfig vtnet1 inet 192.0.2.11/31
ifconfig vtnet2 inet 192.0.2.13/31
ifconfig vtnet2 inet 203.0.113.14/32 alias
ifconfig vtnet3 inet 192.0.2.15/31
ifconfig vtnet3 inet 203.0.113.14/32 alias
route add -net 203.0.113.15/32 192.0.2.12 -ifa 203.0.113.14 -ifp vtnet2
route add -net 203.0.113.15/32 192.0.2.14 -ifa 203.0.113.14 -ifp vtnet3

# RADIX_MPATH option is not enabled by default...
#  svn checkout https://svn.FreeBSD.org/base/release/11.2.0 /usr/src

# Tentative for source address selection are breaking stuff (next-hop
# interface and source address selection are tied together).
