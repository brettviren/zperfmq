################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################

from __future__ import print_function
import os, sys
from ctypes import *
from ctypes.util import find_library

# zperfmq
lib = None
# check to see if the shared object was embedded locally, attempt to load it
# if not, try to load it using the default system paths...
# we need to use os.chdir instead of trying to modify $LD_LIBRARY_PATH and reloading the interpreter
t = os.getcwd()
p = os.path.join(os.path.dirname(__file__), '..')  # find the path to our $project_ctypes.py
os.chdir(p)  # change directories briefly

try:
    from zperfmq import libzperfmq                  # attempt to import the shared lib if it exists
    lib = CDLL(libzperfmq.__file__)          # if it exists try to load the shared lib
except ImportError:
    pass
finally:
    os.chdir(t)  # switch back to orig dir

if not lib:
    try:
        # If LD_LIBRARY_PATH or your OSs equivalent is set, this is the only way to
        # load the library.  If we use find_library below, we get the wrong result.
        if os.name == 'posix':
            if sys.platform == 'darwin':
                lib = cdll.LoadLibrary('libzperfmq.0.dylib')
            else:
                lib = cdll.LoadLibrary("libzperfmq.so.0")
        elif os.name == 'nt':
            lib = cdll.LoadLibrary('libzperfmq.dll')
    except OSError:
        libpath = find_library("zperfmq")
        if not libpath:
            raise ImportError("Unable to find libzperfmq")
        lib = cdll.LoadLibrary(libpath)

class zperf_node_t(Structure):
    pass # Empty - only for type checking
zperf_node_p = POINTER(zperf_node_t)

class zperf_t(Structure):
    pass # Empty - only for type checking
zperf_p = POINTER(zperf_t)


# zperf_node
lib.zperf_node_new.restype = zperf_node_p
lib.zperf_node_new.argtypes = [c_char_p]
lib.zperf_node_destroy.restype = None
lib.zperf_node_destroy.argtypes = [POINTER(zperf_node_p)]
lib.zperf_node_server.restype = None
lib.zperf_node_server.argtypes = [zperf_node_p, c_char_p]
lib.zperf_node_test.restype = None
lib.zperf_node_test.argtypes = [c_bool]

class ZperfNode(object):
    """

    """

    allow_destruct = False
    def __init__(self, *args):
        """
        Create a new zperf_node.
        """
        if len(args) == 2 and type(args[0]) is c_void_p and isinstance(args[1], bool):
            self._as_parameter_ = cast(args[0], zperf_node_p) # Conversion from raw type to binding
            self.allow_destruct = args[1] # This is a 'fresh' value, owned by us
        elif len(args) == 2 and type(args[0]) is zperf_node_p and isinstance(args[1], bool):
            self._as_parameter_ = args[0] # Conversion from raw type to binding
            self.allow_destruct = args[1] # This is a 'fresh' value, owned by us
        else:
            assert(len(args) == 1)
            self._as_parameter_ = lib.zperf_node_new(args[0]) # Creation of new raw type
            self.allow_destruct = True

    def __del__(self):
        """
        Destroy the zperf_node.
        """
        if self.allow_destruct:
            lib.zperf_node_destroy(byref(self._as_parameter_))

    def __eq__(self, other):
        if type(other) == type(self):
            return other.c_address() == self.c_address()
        elif type(other) == c_void_p:
            return other.value == self.c_address()

    def c_address(self):
        """
        Return the address of the object pointer in c.  Useful for comparison.
        """
        return addressof(self._as_parameter_.contents)

    def __bool__(self):
        "Determine whether the object is valid by converting to boolean" # Python 3
        return self._as_parameter_.__bool__()

    def __nonzero__(self):
        "Determine whether the object is valid by converting to boolean" # Python 2
        return self._as_parameter_.__nonzero__()

    def server(self, nickname):
        """
        Create a server in the node.
        """
        return lib.zperf_node_server(self._as_parameter_, nickname)

    @staticmethod
    def test(verbose):
        """
        Self test of this class.
        """
        return lib.zperf_node_test(verbose)


# zperf
lib.zperf_new.restype = zperf_p
lib.zperf_new.argtypes = [c_int]
lib.zperf_destroy.restype = None
lib.zperf_destroy.argtypes = [POINTER(zperf_p)]
lib.zperf_bind.restype = c_char_p
lib.zperf_bind.argtypes = [zperf_p, c_char_p]
lib.zperf_connect.restype = c_int
lib.zperf_connect.argtypes = [zperf_p, c_char_p]
lib.zperf_measure.restype = c_long
lib.zperf_measure.argtypes = [zperf_p, c_char_p, c_int, c_long]
lib.zperf_initiate.restype = None
lib.zperf_initiate.argtypes = [zperf_p, c_char_p, c_int, c_long]
lib.zperf_finalize.restype = c_long
lib.zperf_finalize.argtypes = [zperf_p]
lib.zperf_name.restype = c_char_p
lib.zperf_name.argtypes = [zperf_p]
lib.zperf_nmsgs.restype = c_int
lib.zperf_nmsgs.argtypes = [zperf_p]
lib.zperf_msgsize.restype = c_long
lib.zperf_msgsize.argtypes = [zperf_p]
lib.zperf_noos.restype = c_int
lib.zperf_noos.argtypes = [zperf_p]
lib.zperf_bytes.restype = c_long
lib.zperf_bytes.argtypes = [zperf_p]
lib.zperf_cpu.restype = c_long
lib.zperf_cpu.argtypes = [zperf_p]
lib.zperf_time.restype = c_long
lib.zperf_time.argtypes = [zperf_p]
lib.zperf_test.restype = None
lib.zperf_test.argtypes = [c_bool]

class Zperf(object):
    """
    API to the zperfmq perf actor.
    """

    allow_destruct = False
    def __init__(self, *args):
        """
        Create a new zperf using the given socket type.
        """
        if len(args) == 2 and type(args[0]) is c_void_p and isinstance(args[1], bool):
            self._as_parameter_ = cast(args[0], zperf_p) # Conversion from raw type to binding
            self.allow_destruct = args[1] # This is a 'fresh' value, owned by us
        elif len(args) == 2 and type(args[0]) is zperf_p and isinstance(args[1], bool):
            self._as_parameter_ = args[0] # Conversion from raw type to binding
            self.allow_destruct = args[1] # This is a 'fresh' value, owned by us
        else:
            assert(len(args) == 1)
            self._as_parameter_ = lib.zperf_new(args[0]) # Creation of new raw type
            self.allow_destruct = True

    def __del__(self):
        """
        Destroy the zperf.
        """
        if self.allow_destruct:
            lib.zperf_destroy(byref(self._as_parameter_))

    def __eq__(self, other):
        if type(other) == type(self):
            return other.c_address() == self.c_address()
        elif type(other) == c_void_p:
            return other.value == self.c_address()

    def c_address(self):
        """
        Return the address of the object pointer in c.  Useful for comparison.
        """
        return addressof(self._as_parameter_.contents)

    def __bool__(self):
        "Determine whether the object is valid by converting to boolean" # Python 3
        return self._as_parameter_.__bool__()

    def __nonzero__(self):
        "Determine whether the object is valid by converting to boolean" # Python 2
        return self._as_parameter_.__nonzero__()

    def bind(self, address):
        """
        Bind the zperf measurement socket to an address.
Return the qualified address or NULL on error.
        """
        return lib.zperf_bind(self._as_parameter_, address)

    def connect(self, address):
        """
        Connect the zperf measurement socket to a fully qualified address.
Return code is zero on success.
        """
        return lib.zperf_connect(self._as_parameter_, address)

    def measure(self, name, nmsgs, msgsize):
        """
        Perform a measurement atomically.  This is simply the combination
of initialize() and finalize().
        """
        return lib.zperf_measure(self._as_parameter_, name, nmsgs, msgsize)

    def initiate(self, name, nmsgs, msgsize):
        """
        Initiate a measurement.
        """
        return lib.zperf_initiate(self._as_parameter_, name, nmsgs, msgsize)

    def finalize(self):
        """
        Wait for the previously initiated a measurement.
        """
        return lib.zperf_finalize(self._as_parameter_)

    def name(self):
        """
        Return the name of the last measurement.
        """
        return lib.zperf_name(self._as_parameter_)

    def nmsgs(self):
        """
        The requested number of message for last measurement.
        """
        return lib.zperf_nmsgs(self._as_parameter_)

    def msgsize(self):
        """
        The requested size of message for last measurement.
        """
        return lib.zperf_msgsize(self._as_parameter_)

    def noos(self):
        """
        Return the number of messages that were received out of sync
during the previous yodel or recv measurements.  The measurement
must be finalized.
        """
        return lib.zperf_noos(self._as_parameter_)

    def bytes(self):
        """
        Return the number of bytes transferred by the previous
measurement.  The measurement must be finalized.
        """
        return lib.zperf_bytes(self._as_parameter_)

    def cpu(self):
        """
        Return the CPU time (user+system) in microseconds used by the last
measurement.  The measurement must be finalized.
        """
        return lib.zperf_cpu(self._as_parameter_)

    def time(self):
        """
        Return the elapsed time in microseconds used by the last
measurement.  The measurement must be finalized.
        """
        return lib.zperf_time(self._as_parameter_)

    @staticmethod
    def test(verbose):
        """
        Self test of this class.
        """
        return lib.zperf_test(verbose)

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
