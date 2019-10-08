from setuptools import setup
from setuptools import setup

setup(
    name='zperfmq',
    description="ZeroMQ Performance Measurements",
    version='0.0.0',
    author = 'Brett Viren',
    author_email = 'brett.viren@gmail.com',
    license = 'LGPLv3',
    url='https://github.com/brettviren/zperfmq',
    #packages=['zperfmq'],
    package_dir={'': 'bindings/python'},
    py_modules= ['zperfmq', 'zperfmq.main'],
    install_requires = [l for l in open("requirements.txt").readlines() if l.strip()],
    entry_points = {
        'console_scripts': [
            'zperf = zperfmq.main:main',
        ]
    }
)
