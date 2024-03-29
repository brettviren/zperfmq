################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
from setuptools import setup

setup(
    name = "zperfmq_cffi",
    version = "0.0.0",
    license = "lgpl 3",
    description = """Python cffi bindings of: zeromq performance measurement tool""",
    packages = ["zperfmq_cffi", ],
    setup_requires=["cffi"],
    cffi_modules=[
           "zperfmq_cffi/build.py:ffibuilder",
           "zperfmq_cffi/build.py:ffidestructorbuilder"
    ],
    install_requires=["cffi"],
)
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
