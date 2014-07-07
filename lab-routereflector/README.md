Route reflectors with JunOS
===========================

This lab uses Firefly Perimeter (virtualized Juniper SRX). See
`lab-firefly` for more information about that aspect.

We experiment on two different AS. AS65536 is using route reflectors
while AS65537 doesn't.

`T1` and `T2` are two distinct transit providers. Some routes are only
accessible from `T1` (those in `9.9.0.0/16`) or from `T2` (those in
`7.7.0.0/16`). Other routes (those in `8.8.0.0/16`) are accessible
through either `T1` and `T2`:

 - `8.8.4.0/24` is advertised with the same AS path from T1 and T2
 - `8.8.5.0/24` is advertised with a shorter AS path from T2 (prefixed by `65548` from T1)
 - `8.8.6.0/24` is advertised with a shorter AS path from T1 (prefixed by `65549` from T2)
 - `8.8.7.0/24` is advertised with a shorter AS path from T2 (prefixed by `65546` from T1)
 - `8.8.8.0/24` is advertised with a shorter AS path from T1 (prefixed by `65547` from T2)

The AS are:

 - `65550` for T1
 - `65551` for T2
 - `65536` for the site using route reflection
 - `65537` for the site not using route reflection

Edge routers are configured to force transit to `65546` through `T1`
and transit to `65547` through `T2` (despite the fact that the path is
longer). This is done by setting up MED.
