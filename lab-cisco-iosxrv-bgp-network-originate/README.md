# IOS XRv policy effect on network/originated routes

The goal is to check if a route from a network clause is subject to
out policy. Same for `default-originate`. The conclusion is that
`network` directives are subject to further filtering while
`default-originate` will not.
