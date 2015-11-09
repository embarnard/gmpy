## Todo List ##

MPFR support is not thread-safe.

MPC support has been started but isn't yet usable.

A new, modern test suite needs to be written.

Random number support needs to be added.

format() still needs to be added to mpq.

## Gotchas ##

If you enable trap\_erange, you will get an exception when you compare against "nan". However, you can still use mpfr("nan") as a key in a dictionary but you get the exception when you try to access the dictionary by that key (and probably any number that hashes to the same value).