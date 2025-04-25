# SALOME Geometry module

This directory contains code from the [SALOME platform][salome],
specifically `GEOMAlgo_Gluer2` from the [`geom` module][geom.git].

Looking at the history in Git it seems to be very stable with only a
single fix in the past decade. Hence I've extracted the self-contained
portion of the code that is likely needed to understand it better.

Currently this merging process is only done serially, in the hope that
feedback from using this on "real" geometries will show where it's
worth optimising things and where parallelisation would help.

[salome]: http://www.salome-platform.org/
[geom.git]: https://git.salome-platform.org/gitweb/?p=modules/geom.git
