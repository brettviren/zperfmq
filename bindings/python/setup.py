import os.path
from _project import NAME, VERSION, LICENSE, DESCRIPTION, URL, PACKAGES, REQUIRES
from setuptools import setup

setup(
    name = NAME,
    version = VERSION,
    license = LICENSE,
    description = DESCRIPTION,
    url = URL,
    packages = PACKAGES,
    install_requires = REQUIRES,
)
