# Test with crpd from Juniper

Juniper provides a tarball to import into Docker:

    docker image load -i junos-routing-crpd-docker-19.2R1.8.tgz

To get a `crpd.img`:

    container=$(docker create crpd:19.2R1.8)
    virt-make-fs --partition --format=qcow2 =(docker container export $container) crpd.img
    docker container rm $container
