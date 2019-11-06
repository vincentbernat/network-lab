proxy:
  proxytype: napalm
  driver: junos
  host: {{ grains.id }}
  username: juniper
  password: ''
  optional_args:
    key_file: /tmp/lab/id_rsa
