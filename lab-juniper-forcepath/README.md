# Force a destination to a given path

In presence of multiple possible paths, this is a tentative to force a
destination through one path and the other through the other path.
Each path is over a dedicated virtual router as it is not possible to
match individual paths of an ECMP route or an inactive route. This
also matches our reality where you usually have two routers with two
distinct transits.

However, this doesn't work. Contributing route need to be included in
the generated route. We cannot generate a route from an unrelated or
shorter prefix.
