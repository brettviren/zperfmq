################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
#zperfmq cffi bindings

This package contains low level python bindings for zperfmq based on cffi library.
Module is compatible with
 * The “in-line”, “ABI mode”, which simply **dlopen** main library and parse C declaration on runtime
 * The “out-of-line”, “API mode”, which build C **native** Python extension

#Build the native extension

    python setup.py build

Note you need to have setuptools and cffi packages installed. As well as a checkout of all dependencies
at the same level as this project, because all dependant defs.py will be included in project cdefs.py.

#Using more cffi modules together
While zproject and CLASS encourages you to split your dependencies to smaller libraries, this does
not work well for cffi. zproject generated backends have own private cffi instance, which can't
be easily combined with others in one function call.

See ML thread about topic https://groups.google.com/forum/#!topic/python-cffi/JtAKU-g9Exg

This is the reason the Lib and CompiledFFi objects are referenced from utils module and dynamically
accessed on each call. Calling `lower.utils.rebind (higher.utils.lib, higher.utils.ffi)' you can
change instance used by lower module and enforce all Python classes will use single instance.
