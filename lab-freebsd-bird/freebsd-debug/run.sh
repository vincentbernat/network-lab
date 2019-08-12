#!/bin/sh

cat <<EOF > /dev/null
# To provision:
mkdir /mnt/lab
mount -t msdosfs /dev/vtbd1 /mnt/lab
sh /mnt/lab/run.sh
EOF

set -e
cd $(dirname $0)
hostname freebsd-debug

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
env ASSUME_ALWAYS_YES=YES pkg install zsh curl mg gdb socat
curl -s https://vincentbernat-zshrc.s3.amazonaws.com/zsh-install.sh | sh
