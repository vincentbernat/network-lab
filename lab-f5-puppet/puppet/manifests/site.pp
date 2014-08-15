node "f5.local" {

  f5_pool { 'webfront':
    ensure => present,
    action_on_service_down => 'SERVICE_DOWN_ACTION_NONE',
    lb_method => 'LB_METHOD_ROUND_ROBIN',
    member => {
      '10.10.0.1:80' => {},
      '10.10.0.2:80' => {}
    }
  }

}
