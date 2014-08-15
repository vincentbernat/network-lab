Puppet with F5 devices
=========================

This lab features the use of Puppet with F5 devices using "puppet
device" with the following [appropriate Puppet module][1].

[1]: https://github.com/puppetlabs/puppetlabs-f5

There is no provision in this lab to get access to the web interface
(a SLIRP VDE process would enable that). However, everything should be
available through the command-line with `tmsh`.

You need a valid license. Be careful, each time you reboot the lab,
the license needs to be reloaded. From the command line, invoke
`get_dossier`:

    get_dossier -b XXXXX-XXXXX-XXXXX-XXXXX-XXXXXXX

[Activate the license][3].

[3]: https://activate.f5.com/license/dossier.jsp

A license needs to be enabled with the following `tmsh` command:

    install /sys license registration-key 

On puppet, the F5 can be provisioned with:

    puppet device --debug

Once provisioned, you can get the collected facts in
`/var/lib/puppet/yaml/facts/f5.local.yaml`. You can also check with
`tmsh` that the provisioning has been done correctly:

You can find more info in the related [Puppetlabs blog post][2].

[2]: http://puppetlabs.com/blog/managing-f5-big-ip-network-devices-with-puppet
