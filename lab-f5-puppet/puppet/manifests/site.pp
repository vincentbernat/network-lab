node "f5.local" {

  f5_pool { '/Common/webfront':
    ensure => present,
    action_on_service_down => 'SERVICE_DOWN_ACTION_NONE',
    lb_method => 'LB_METHOD_ROUND_ROBIN',
    allow_nat_state => 'STATE_ENABLED',
    allow_snat_state => 'STATE_ENABLED',
    minimum_active_member => '1',
    minimum_up_member => '0',
    member => {
      '/Common/10.10.0.1:80' => {
        "connection_limit"=>"0",
        "dynamic_ratio"=>"1",
        "priority"=>"0",
        "ratio"=>"1"
      },
      '/Common/10.10.0.2:80' => {
        "connection_limit"=>"0",
        "dynamic_ratio"=>"1",
        "priority"=>"0",
        "ratio"=>"1"
      }
    }
  }

}
