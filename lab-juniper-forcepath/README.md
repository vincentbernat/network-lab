# Force a destination to a given path

In presence of an ECMP route, this is a tentative to force a
destination through one path and the other through the other path.
There are several problems:

 1. Contributing route need to be included in the generated route. We
    cannot generate a route from an unrelated or shorter prefix. So,
    it doesn't work.

 2. ECMP routes contributing to a generated route use all their next
    hops, not just the one selected.
