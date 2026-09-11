# Babbage

Babbage is LALVM's own Ada runtime and it is meant to be compiled by LALVM.
Until it is, use GNAT to check that the sources are valid Ada with

```sh
gprbuild -P babbage.gpr -cargs -gnatg
```
